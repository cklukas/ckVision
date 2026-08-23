// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The geometry every scrolling text surface needs, in one place: where a
// logical line breaks into display rows, and which scrollbars a viewport
// shows once each bar's own presence is accounted for.
//
// Both were written three times before this existed — once in TextView, once
// in Memo, once in TextEditor — and the three disagreed. Two broke lines
// mid-word where the third broke at spaces; only one resolved its scrollbars
// against each other. A reader moving between a help page, a memo field and
// an editor met three behaviours that ought to be one.
#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/geometry.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

// One display row's worth of a logical line, as a half-open range. The units
// are whatever the caller indexed by: grapheme indices from wrap_graphemes,
// byte offsets from wrap_text.
struct WrapSegment {
    std::size_t begin = 0;
    std::size_t end = 0;

    friend bool operator==(const WrapSegment&, const WrapSegment&) = default;
};

// Where a line is allowed to break. All three are offered because each is
// right for something and wrong for the others.
enum class WrapMode {
    // One display row per logical line, however long. Horizontal scrolling
    // reaches the rest. Preformatted text — a table, a diagram, code, a log —
    // means what it means only at its own line breaks, and rewrapping it
    // destroys the alignment that carried the meaning.
    None,
    // Break between words, keeping each word whole. What prose wants, and
    // the sensible default wherever wrapping is turned on at all. A word
    // wider than the row is NOT split: it takes a row of its own and
    // overflows, which is what makes a horizontal bar appear rather than
    // telling the reader a path is shorter than it is.
    Word,
    // Break exactly at the edge, mid-word where the edge falls there. For
    // content with no word structure to respect — a hex dump, one unbroken
    // identifier, a script that does not space its words — where filling
    // every column matters more than keeping tokens intact.
    Character,
};

// How a line is allowed to break.
struct WrapOptions {
    // Cells available to a display row. Zero or less means no wrapping.
    int width = 0;
    WrapMode mode = WrapMode::None;
    // Cells held back on every row of a wrapped line, for a continuation
    // marker drawn by the caller. Ignored when it would leave no room.
    int continuation_reserve = 0;
};

// Where `graphemes` breaks into display rows. Always at least one segment,
// so an empty line still occupies a row.
//
// A word wider than the whole width is NOT broken: it takes a row of its own
// and overflows. Breaking mid-word would hide that a path or an identifier is
// wider than the window, and it is what makes the horizontal bar appear under
// ScrollbarPolicy::Auto even with wrapping on.
std::vector<WrapSegment> wrap_graphemes(std::span<const std::string> graphemes, const WrapOptions& options);

// The same rule over a UTF-8 string, returning BYTE offsets — for a surface
// that stores lines as text rather than as split clusters. Cluster boundaries
// are respected; a segment never splits one.
std::vector<WrapSegment> wrap_text(std::string_view text, const WrapOptions& options);

// Which bars a scrolling surface shows, and what is left for its content.
struct ScrollGeometry {
    bool show_vertical = false;
    bool show_horizontal = false;
    // Bounds minus whichever bars are showing. Always measure content and
    // draw against these, never against the raw bounds, or the two will
    // disagree about where the content ends.
    int viewport_width = 0;
    int viewport_height = 0;
};

// Resolves both bars together.
//
// They cannot be decided independently: a vertical bar costs a column, which
// can be what makes a line no longer fit; a horizontal bar costs a row, which
// can be what makes the text no longer fit; and with word wrap the width also
// decides how many display rows there are. Deciding them one at a time gives
// a view that either overlaps its own content or hides a row nothing can
// scroll to.
//
// `measure` reports the content size for a candidate viewport width, and is
// called more than once — it must be a pure measurement, not a mutation.
ScrollGeometry resolve_scroll_geometry(Size bounds, ScrollbarPolicy vertical, ScrollbarPolicy horizontal,
                                        const std::function<Size(int viewport_width)>& measure);

}  // namespace ckv::widgets
