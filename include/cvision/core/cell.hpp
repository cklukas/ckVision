// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>

#include "cvision/core/export.hpp"
#include "cvision/core/hyperlink.hpp"
#include "cvision/core/style.hpp"

namespace ckv {

namespace scene {
class Painter;
}

// One grapheme cluster, its style, its precomputed column width, and the
// hyperlink it belongs to, if any — the scene layer's atomic drawing unit
// (ARCHITECTURE.md §2, DECISIONS.md D-156).
//
// Packed to exactly 32 bytes with 15-byte inline UTF-8 storage; rare > 15-byte
// grapheme clusters (e.g. long multi-ZWJ emoji) share an immutable refcounted
// heap block with zero global state.
class CVISION_API Cell {
public:
    // A blank: one space in the default Style, one column wide.
    Cell() noexcept = default;

    // Copies the cell, retaining the shared heap grapheme block when > 15 bytes.
    Cell(const Cell& other) noexcept
        : len_and_flags_(other.len_and_flags_), link_(other.link_), style_(other.style_) {
        std::memcpy(bytes_, other.bytes_, sizeof(bytes_));
        if ((len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            retain_heap();
        }
    }

    // Replaces this cell with a copy of `other`, updating heap refcounts safely.
    Cell& operator=(const Cell& other) noexcept {
        if (this == &other) return *this;
        if ((other.len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            other.retain_heap();
        }
        if ((len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            release_heap();
        }
        std::memcpy(bytes_, other.bytes_, sizeof(bytes_));
        len_and_flags_ = other.len_and_flags_;
        link_ = other.link_;
        style_ = other.style_;
        return *this;
    }

    // Moves `other` into this cell without touching heap reference counts.
    Cell(Cell&& other) noexcept
        : len_and_flags_(other.len_and_flags_), link_(other.link_), style_(other.style_) {
        std::memcpy(bytes_, other.bytes_, sizeof(bytes_));
        if ((other.len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            other.len_and_flags_ = 0;
        }
    }

    // Move-assigns `other` into this cell without allocating or incrementing refs.
    Cell& operator=(Cell&& other) noexcept {
        if (this == &other) return *this;
        if ((len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            release_heap();
        }
        std::memcpy(bytes_, other.bytes_, sizeof(bytes_));
        len_and_flags_ = other.len_and_flags_;
        link_ = other.link_;
        style_ = other.style_;
        if ((other.len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            other.len_and_flags_ = 0;
        }
        return *this;
    }

    // Releases the shared heap grapheme block if this cell holds one.
    ~Cell() {
        if ((len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            release_heap();
        }
    }

    // `grapheme` need not already be sanitized or a single verified
    // cluster: this factory sanitizes it (DECISIONS.md D-040 — a Cell
    // can never carry a raw control character regardless of caller
    // discipline) and keeps only the first resulting grapheme cluster,
    // since a Cell holds exactly one. An empty `grapheme` yields a space. The
    // width is computed from the kept cluster: 0, 1 or 2 columns.
    static CVISION_API Cell from_grapheme(std::string_view grapheme, Style style);

    // A zero-width, empty-grapheme placeholder marking the second (and
    // further) column of a wide grapheme drawn in the preceding column.
    // Never drawn on its own — Surface/Painter/Compositor treat it as
    // "covered by the predecessor", not as its own paintable cell.
    // Distinguishable from a genuine zero-width glyph (e.g. a lone
    // combining mark with no base, which has width 0 but a non-empty
    // grapheme) by is_continuation().
    static Cell continuation(Style style) noexcept {
        return Cell(ContinuationTag{}, style);
    }

    // True when this cell is the trailing continuation column of a wide glyph.
    bool is_continuation() const noexcept { return len_and_flags_ == 0; }

    // The cell's UTF-8 grapheme cluster; empty only for a continuation. The
    // view points into this Cell (or its shared heap block) and is invalidated
    // when the Cell changes or is destroyed.
    std::string_view grapheme() const noexcept {
        if ((len_and_flags_ & kHeapFlag) != 0) [[unlikely]] {
            return heap_view();
        }
        return std::string_view(bytes_, static_cast<std::size_t>(len_and_flags_ & kLenMask));
    }

    // The style the grapheme is drawn in. set_style replaces it and leaves
    // the grapheme and width untouched, which is how style-only passes such
    // as shadows restyle existing content.
    const Style& style() const noexcept { return style_; }
    void set_style(Style style) noexcept { style_ = style; }

    // Columns the grapheme occupies: 1 or 2 for ordinary content, 0 for a
    // continuation or a lone zero-width cluster.
    int width() const noexcept {
        return static_cast<int>((len_and_flags_ & kWidthMask) >> kWidthShift);
    }

    // The hyperlink the cell is part of, as an id into the LinkTable of the
    // grid that holds it (Surface::links(), FrameView::links()); kNoLink for
    // a cell that is not a link, which is what every factory above makes.
    LinkId link() const noexcept { return link_; }
    void set_link(LinkId link) noexcept { link_ = link; }

    // Whether grapheme, style and width match, whatever the links: how a
    // cell from one grid is compared with a cell from another, whose link ids
    // cannot be compared (same_cell in frame_view.hpp adds the targets).
    bool same_content(const Cell& other) const noexcept {
        return width() == other.width() && style_ == other.style_ && grapheme() == other.grapheme();
    }

    // Equal when grapheme, style, width and link id all match — a comparison
    // of two cells of one grid. Across grids, use same_cell (frame_view.hpp).
    friend bool operator==(const Cell& a, const Cell& b) noexcept {
        return a.link_ == b.link_ && a.same_content(b);
    }

private:
    struct ContinuationTag {};

    static constexpr std::uint8_t kLenMask = 0x0Fu;
    static constexpr std::uint8_t kWidthShift = 5u;
    static constexpr std::uint8_t kWidthMask = 0x60u;
    static constexpr std::uint8_t kHeapFlag = 0x80u;
    static constexpr std::size_t kMaxInlineBytes = sizeof(char[15]);

    Cell(ContinuationTag, Style style) noexcept
        : bytes_{}, len_and_flags_(0), link_(kNoLink), style_(style) {}

    friend class scene::Painter;
    static CVISION_API Cell from_sanitized_grapheme(std::string_view cluster, std::uint8_t width,
                                                    Style style);

    CVISION_API void retain_heap() const noexcept;
    CVISION_API void release_heap() noexcept;
    CVISION_API std::string_view heap_view() const noexcept;

    char bytes_[15]{' '};
    std::uint8_t len_and_flags_ = static_cast<std::uint8_t>((1u << kWidthShift) | 1u);
    LinkId link_ = kNoLink;
    Style style_{};
};

static_assert(sizeof(Cell) == 32, "Cell must be 32 bytes (D-156)");

}  // namespace ckv
