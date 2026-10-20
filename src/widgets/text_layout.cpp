// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/text_layout.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {
namespace {

struct WrapPiece { std::size_t end; int width; bool space; };

// Both byte and cluster entry points use this rule. A piece cursor replaces
// the old whole-line intermediate vector, so one-row layout allocates nothing.
template<class PieceAt>
WrapSegment next_row(std::size_t begin, std::size_t size, const WrapOptions& options, PieceAt piece_at) {
    begin = std::min(begin, size);
    if (options.mode == WrapMode::None || options.width <= 0) return {begin, size};
    const int usable = options.continuation_reserve > 0 && options.width > options.continuation_reserve
        ? options.width - options.continuation_reserve : options.width;
    std::size_t end = begin, last_break = begin;
    int width = 0;
    while (end < size) {
        const auto piece = piece_at(end);
        if (piece.width > usable - width) break;
        width += piece.width;
        end = piece.end;
        if (end < size && piece.space) last_break = end;
    }
    if (end == size) return {begin, end};
    if (options.mode == WrapMode::Word && last_break > begin) end = last_break;
    else if (end == begin) end = piece_at(begin).end;
    else if (options.mode == WrapMode::Word) {
        while (end < size) {
            const auto piece = piece_at(end);
            if (piece.space) break;
            end = piece.end;
        }
    }
    return {begin, end};
}

}  // namespace

std::vector<WrapSegment> wrap_graphemes(std::span<const std::string> graphemes, const WrapOptions& options) {
    std::vector<WrapSegment> segments;
    std::size_t begin = 0;
    do {
        const auto segment = next_row(begin, graphemes.size(), options, [graphemes](std::size_t at) {
            return WrapPiece{at + 1, std::max(1, text::grapheme_width(graphemes[at])), graphemes[at] == " "};
        });
        segments.push_back(segment);
        begin = segment.end;
    } while (begin < graphemes.size());
    return segments;
}

WrapSegment next_text_row(std::string_view value, std::size_t begin, const WrapOptions& options) noexcept {
    return next_row(begin, value.size(), options, [value](std::size_t at) {
        const auto end = text::grapheme_end(value, at);
        const auto cluster = value.substr(at, end - at);
        return WrapPiece{end, std::max(1, text::grapheme_width(cluster)), cluster == " "};
    });
}

std::vector<WrapSegment> wrap_text(std::string_view value, const WrapOptions& options) {
    std::vector<WrapSegment> segments;
    std::size_t begin = 0;
    do {
        const auto segment = next_text_row(value, begin, options);
        segments.push_back(segment);
        begin = segment.end;
    } while (begin < value.size());
    return segments;
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
