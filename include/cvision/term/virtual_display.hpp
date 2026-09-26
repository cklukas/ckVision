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
#include "cvision/core/hyperlink.hpp"
#include "cvision/core/image.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/term/sixel_decoder.hpp"

namespace ckv::term {

// A strict model of the screen a host would show for a byte stream. It understands exactly
// the subset ckVision emits (cursor addressing and style, SGR, erase, scroll, DEC modes 25
// and 2026, DSR, OSC 0, 8, 22 and 52, and Sixel) and treats anything else as an error rather
// than skipping it, so an emitter that drifts from the model is caught instead of silently
// tolerated. The first error latches: error() keeps its message and every later feed() fails
// until clear() or resize().
//
// The cursor never leaves the grid, as on a terminal (ECMA-48 CUP; the xterm control
// sequences): a cursor position past the page is clamped to its last row or column. Text
// follows the autowrap mode a host starts in and ckVision never changes (DECAWM set): a
// cluster written into the last column leaves the cursor there with a wrap pending, and the
// next cluster, or a wide cluster that would not fit in the columns left, begins the next
// row, scrolling the page up one row from the bottom one. Cursor addressing cancels a
// pending wrap.
//
// Hyperlinks (OSC 8) are modelled as a host that renders them does: text printed while one is
// open becomes part of it, and frame().link_target() says which cells a terminal would make
// clickable. The display holds the stream to ckVision's emission contract (D-088): the target
// must be a valid terminal hyperlink with the id hyperlink_id() derives from it; a hyperlink
// is opened only while none is open; and while one is open nothing but text, SGR and the OSC 8
// that closes it may follow — no cursor addressing, erase, scroll, mode, DCS or other OSC.
class VirtualDisplay {
public:
    // A blank display of `cells` (negative extents clamp to 0) whose cells each cover
    // `cell_pixels` of the raster plane (each extent clamped to at least 1). The cursor starts
    // at the origin with the default style.
    explicit VirtualDisplay(Size cells, PixelSize cell_pixels = PixelSize{9, 18});

    // The grid in cells, the pixel size of one cell, and the raster plane's pixel size (the
    // two multiplied).
    Size size() const noexcept { return size_; }
    PixelSize cell_pixels() const noexcept { return cell_pixels_; }
    PixelSize pixel_size() const noexcept { return cells_to_pixels(size_, cell_pixels_); }

    // What a viewer would see now: the styled cells, the transparent RGBA plane Sixel images
    // were painted into (alpha 0 where no picture drew), and the cursor. Between DEC 2026
    // begin and end these return the state captured when the update began, as a host holding
    // the update back would; the new state appears when the update ends. The FrameView and
    // Image reference internal storage that the next feed(), resize() or clear() may replace.
    FrameView frame() const noexcept {
        return synchronized_output_ ? FrameView(visible_cells_.data(), size_, &visible_links_)
                                    : FrameView(cells_.data(), size_, &links_);
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
    // The window title the stream last set with OSC 0, as the host would show it; empty until
    // one is set. A title holding any control code point, DEL, or malformed UTF-8 is refused
    // as an error: a host could read such a byte as the sequence's end or as a new control,
    // and ckVision's emitters never send one (docs/terminal-host-integration.md).
    const std::string& window_title() const noexcept { return window_title_; }
    // The text the stream last exported to the clipboard with OSC 52 on selection `c`,
    // decoded from its base64 payload; empty until one arrives. A payload that is not strict
    // RFC 4648 base64, or names another selection, is refused as an error.
    const std::string& clipboard_text() const noexcept { return clipboard_text_; }

    // Whether any pixel of the visible raster plane is non-transparent, that is whether a
    // Sixel image is currently on screen. Scans the whole plane.
    bool has_raster_pixels() const noexcept;
    // valid() is false once the stream has failed; error() then holds the first failure's
    // message, and is empty while the stream is valid.
    bool valid() const noexcept { return error_.empty(); }
    const std::string& error() const noexcept { return error_; }

    // The target of the hyperlink printed text currently joins, or empty while none is open.
    std::string_view open_hyperlink() const noexcept { return links_.target(pen_link_); }

    // Feeds an arbitrary byte fragment. Parser state is retained across
    // calls, including a split CSI/DCS/Sixel sequence. Printable text is
    // committed whenever a fragment ends in ground state. finish() marks
    // end-of-stream, where an incomplete control sequence, or a hyperlink
    // still open, becomes an error. Both return valid() afterwards.
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
    void set_cell_pixels(PixelSize cell_pixels);
    // Blanks both planes, homes the cursor with the default cursor state and style, closes any
    // open hyperlink and forgets every link, resets the parser and the Sixel colour registers,
    // and clears a latched error. The last
    // pointer shape name, window title and clipboard text are kept: they are host state
    // outside the screen.
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
    bool handle_hyperlink(std::string_view body);
    // Refuses `what` while a hyperlink is open, where ckVision never sends it.
    bool refuse_inside_hyperlink(const char* what);
    // Writes `cell` at `index`, keeping the link table's reference counts.
    void store_cell(std::size_t index, Cell cell) noexcept;
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
    PixelSize cell_pixels_;
    std::vector<Cell> cells_;
    // The targets cells_ link to, one reference per cell plus one for the
    // open hyperlink, and the link printed text joins (kNoLink when none).
    LinkTable links_;
    LinkId pen_link_ = kNoLink;
    Image raster_plane_;
    CursorState cursor_;
    // A cluster reached the last column and the cursor stays on it: the next
    // cluster begins the next row (DECAWM's pending wrap).
    bool wrap_pending_ = false;
    Style style_;
    bool synchronized_output_ = false;
    std::vector<Cell> visible_cells_;
    LinkTable visible_links_;
    Image visible_raster_plane_;
    CursorState visible_cursor_;

    ParseState state_ = ParseState::Ground;
    std::string text_buffer_;
    std::string control_buffer_;
    std::string error_;
    std::string pointer_shape_name_;
    std::string window_title_;
    std::string clipboard_text_;
    SixelPalette sixel_palette_;
};

}  // namespace ckv::term
