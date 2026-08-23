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

class VirtualDisplay {
public:
    explicit VirtualDisplay(Size cells, Size cell_pixels = Size{9, 18});

    Size size() const noexcept { return size_; }
    Size cell_pixels() const noexcept { return cell_pixels_; }
    Size pixel_size() const noexcept {
        return Size{size_.width * cell_pixels_.width, size_.height * cell_pixels_.height};
    }

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

    bool has_raster_pixels() const noexcept;
    bool valid() const noexcept { return error_.empty(); }
    const std::string& error() const noexcept { return error_; }

    // Feeds an arbitrary byte fragment. Parser state is retained across
    // calls, including a split CSI/DCS/Sixel sequence. Printable text is
    // committed whenever a fragment ends in ground state. finish() marks
    // end-of-stream, where an incomplete control sequence becomes an error.
    bool feed(std::string_view bytes);
    bool finish();
    bool write(std::string_view bytes) { return feed(bytes) && finish(); }

    // A terminal resize clears both planes. Changing only cell metrics
    // preserves cells but clears/reallocates the pixel plane: existing
    // raster pixels no longer have a meaningful geometry.
    void resize(Size cells);
    void set_cell_pixels(Size cell_pixels);
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
