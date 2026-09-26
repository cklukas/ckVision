// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Deterministic terminal-output model used by HeadlessTerminal. It
// consumes the exact VT/Sixel bytes Presenter writes and exposes the
// resulting styled-cell and transparent RGBA raster planes for tests,
// golden capture, and documentation screenshots (D-035).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/core/cursor.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/core/image.hpp"
#include "cvision/term/sixel_decoder.hpp"

namespace ckv::term {

// A strict model of the screen a host would show for a byte stream. It understands exactly
// the subset ckVision emits (cursor addressing and style, SGR, erase, scroll, DEC modes 25
// and 2026, DSR, OSC 22 and Sixel) and treats anything else as an error rather than skipping
// it, so an emitter that drifts from the model is caught instead of silently tolerated. The
// first error latches: error() keeps its message and every later feed() fails until clear()
// or resize().
class VirtualDisplay {
public:
    // A blank display of `cells` (negative extents clamp to 0) whose cells each cover
    // `cell_pixels` of the raster plane (each extent clamped to at least 1). The cursor starts
    // at the origin with the default style.
    explicit VirtualDisplay(Size cells, Size cell_pixels = Size{9, 18});

    // The grid in cells, the pixel size of one cell, and the raster plane's pixel size (the
    // two multiplied).
    Size size() const noexcept { return size_; }
    Size cell_pixels() const noexcept { return cell_pixels_; }
    Size pixel_size() const noexcept {
        return Size{size_.width * cell_pixels_.width, size_.height * cell_pixels_.height};
    }

    // What a viewer would see now: the styled cells, the transparent RGBA plane Sixel images
    // were painted into (alpha 0 where no picture drew), and the cursor. Between DEC 2026
    // begin and end these return the state captured when the update began, as a host holding
    // the update back would; the new state appears when the update ends. The FrameView and
    // Image reference internal storage that the next feed(), resize() or clear() may replace.
    FrameView frame() const noexcept {
        return FrameView((synchronized_output_ ? visible_cells_ : cells_).data(), size_);
    }
    const Image& raster_plane() const noexcept {
        return synchronized_output_ ? visible_raster_plane_ : raster_plane_;
    }
    CursorState cursor() const noexcept { return synchronized_output_ ? visible_cursor_ : cursor_; }

    // The mouse pointer shape this stream last asked for, as the host would
    // have received it: the raw OSC 22 name, empty for the protocol's reset.
    // A name rather than a PointerShape because that is what a host is told
    // -- which vocabulary was chosen and which degradation was applied are
    // exactly the things a test needs to be able to see.
    const std::string& pointer_shape_name() const noexcept { return pointer_shape_name_; }

    // Whether any pixel of the visible raster plane is non-transparent, that is whether a
    // Sixel image is currently on screen. Scans the whole plane.
    bool has_raster_pixels() const noexcept;
    // valid() is false once the stream has failed; error() then holds the first failure's
    // message, and is empty while the stream is valid.
    bool valid() const noexcept { return error_.empty(); }
    const std::string& error() const noexcept { return error_; }

    // Feeds an arbitrary byte fragment. Parser state is retained across
    // calls, including a split CSI/DCS/Sixel sequence. Printable text is
    // committed whenever a fragment ends in ground state. finish() marks
    // end-of-stream, where an incomplete control sequence becomes an error.
    // Both return valid() afterwards.
    bool feed(std::string_view bytes);
    bool finish();
    // feed() then finish(): for a buffer that is known to be a complete stream.
    bool write(std::string_view bytes) { return feed(bytes) && finish(); }

    // A terminal resize clears both planes. Changing only cell metrics
    // preserves cells but clears/reallocates the pixel plane: existing
    // raster pixels no longer have a meaningful geometry.
    // Both also end any DEC 2026 update in progress. resize() clamps negative extents to 0
    // and, like clear(), discards a latched error. set_cell_pixels() clamps each extent to at
    // least 1 and does nothing when the metric is unchanged.
    void resize(Size cells);
    void set_cell_pixels(Size cell_pixels);
    // Blanks both planes, homes the cursor with the default cursor state and style, resets
    // the parser and the Sixel colour registers, and clears a latched error. The last
    // pointer shape name is kept.
    void clear();

private:
    enum class ParseState {
        Ground,
        Escape,
        Csi,
        Dcs,
        DcsEscape,
        Osc,
        OscEscape,
    };

    struct Rgba {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        std::uint8_t a = 255;
    };

    // One SGR parameter with its colon-separated sub-parameters. SGR is the
    // only control the Presenter writes them in — the shape of an underline
    // and an underline's colour — so every other control here still reads a
    // plain list of numbers, and a colon in one of those is malformed.
    struct SgrParam {
        int value = 0;
        std::vector<int> subs;
    };

    void fail(std::string message);
    bool flush_text();
    bool handle_csi(char final_byte);
    bool handle_osc(std::string_view body);
    bool decode_sixel(std::string_view body);
    static std::vector<SgrParam> parse_sgr_params(std::string_view text, bool& ok);
    bool apply_sgr(const std::vector<SgrParam>& params);
    void put_grapheme(std::string_view grapheme);
    void erase_cells(int left, int top, int right, int bottom) noexcept;
    void scroll_rows(int rows) noexcept;
    void clear_cell_pixels(int cell_x, int cell_y, int cell_width = 1) noexcept;
    void clear_pixel_rect(std::int64_t left, std::int64_t top, std::int64_t right,
                          std::int64_t bottom) noexcept;
    void begin_synchronized_output();
    void end_synchronized_output() noexcept;
    void discard_synchronized_snapshot() noexcept;

    Size size_;
    Size cell_pixels_;
    std::vector<Cell> cells_;
    Image raster_plane_;
    CursorState cursor_;
    Style style_;
    bool synchronized_output_ = false;
    std::vector<Cell> visible_cells_;
    Image visible_raster_plane_;
    CursorState visible_cursor_;

    ParseState state_ = ParseState::Ground;
    std::string text_buffer_;
    std::string control_buffer_;
    std::string error_;
    std::string pointer_shape_name_;
    SixelPalette sixel_palette_;
};

}  // namespace ckv::term
