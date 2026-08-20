// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/virtual_display.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "cvision/core/text.hpp"
#include "cvision/term/sixel_decoder.hpp"

namespace ckv::term {
namespace {

constexpr std::size_t kMaxControlBytes = 16U * 1024U * 1024U;

bool parse_unsigned(std::string_view text, std::size_t& pos, int& value) noexcept {
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return false;
    int parsed = 0;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        const int digit = text[pos] - '0';
        if (parsed > (std::numeric_limits<int>::max() - digit) / 10) return false;
        parsed = parsed * 10 + digit;
        ++pos;
    }
    value = parsed;
    return true;
}

std::vector<int> parse_csi_params(std::string_view text, bool& ok) {
    std::vector<int> result;
    ok = true;
    if (text.empty()) return result;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        if (pos == text.size()) {
            result.push_back(0);
            break;
        }
        if (text[pos] == ';') {
            result.push_back(0);
            ++pos;
            continue;
        }
        int value = 0;
        if (!parse_unsigned(text, pos, value)) {
            ok = false;
            return {};
        }
        result.push_back(value);
        if (pos == text.size()) break;
        if (text[pos] != ';') {
            ok = false;
            return {};
        }
        ++pos;
    }
    return result;
}

}  // namespace

std::vector<VirtualDisplay::SgrParam> VirtualDisplay::parse_sgr_params(std::string_view text,
                                                                        bool& ok) {
    std::vector<SgrParam> result;
    ok = true;
    if (text.empty()) return result;
    std::size_t pos = 0;
    for (;;) {
        SgrParam parameter;
        for (bool first = true;; first = false) {
            int value = 0;
            // An omitted number means zero, which is how `58:2::R:G:B`
            // spells "no colour space named".
            if (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
                if (!parse_unsigned(text, pos, value)) {
                    ok = false;
                    return {};
                }
            }
            if (first) parameter.value = value;
            else parameter.subs.push_back(value);
            if (pos == text.size() || text[pos] != ':') break;
            ++pos;
        }
        result.push_back(std::move(parameter));
        if (pos == text.size()) break;
        if (text[pos] != ';') {
            ok = false;
            return {};
        }
        ++pos;
        if (pos == text.size()) {
            result.push_back(SgrParam{});
            break;
        }
    }
    return result;
}

namespace {

Attr without_attr(Attr attrs, Attr removed) noexcept {
    const auto bits = static_cast<std::uint8_t>(attrs);
    const auto mask = static_cast<std::uint8_t>(removed);
    return static_cast<Attr>(bits & static_cast<std::uint8_t>(~mask));
}


}  // namespace

VirtualDisplay::VirtualDisplay(Size cells, Size cell_pixels)
    : size_{std::max(0, cells.width), std::max(0, cells.height)},
      cell_pixels_{std::max(1, cell_pixels.width), std::max(1, cell_pixels.height)},
      cells_(static_cast<std::size_t>(size_.width) * static_cast<std::size_t>(size_.height)),
      raster_plane_(size_.width * cell_pixels_.width, size_.height * cell_pixels_.height) {
    cursor_.position = Point{0, 0};
}

bool VirtualDisplay::has_raster_pixels() const noexcept {
    const Image& plane = raster_plane();
    for (int y = 0; y < plane.height(); ++y)
        for (int x = 0; x < plane.width(); ++x)
            if (plane.pixel(x, y).a != 0) return true;
    return false;
}

void VirtualDisplay::fail(std::string message) {
    if (error_.empty()) error_ = std::move(message);
}

bool VirtualDisplay::feed(std::string_view bytes) {
    if (!valid()) return false;

    for (const char raw : bytes) {
        const unsigned char byte = static_cast<unsigned char>(raw);
        switch (state_) {
            case ParseState::Ground:
                if (byte == 0x1B) {
                    if (!flush_text()) return false;
                    state_ = ParseState::Escape;
                } else if (byte < 0x20 || byte == 0x7F) {
                    fail("unsupported C0 byte in virtual-display text stream");
                    return false;
                } else {
                    text_buffer_ += raw;
                    if (text_buffer_.size() > kMaxControlBytes) {
                        fail("virtual-display text run exceeds bounded input size");
                        return false;
                    }
                }
                break;

            case ParseState::Escape:
                if (raw == '[') {
                    control_buffer_.clear();
                    state_ = ParseState::Csi;
                } else if (raw == 'P') {
                    control_buffer_.clear();
                    state_ = ParseState::Dcs;
                } else if (raw == ']') {
                    control_buffer_.clear();
                    state_ = ParseState::Osc;
                } else {
                    fail("unsupported ESC sequence in virtual display");
                    return false;
                }
                break;

            case ParseState::Csi:
                if (byte >= 0x40 && byte <= 0x7E) {
                    if (!handle_csi(raw)) return false;
                    control_buffer_.clear();
                    state_ = ParseState::Ground;
                } else if (byte >= 0x20 && byte <= 0x3F) {
                    control_buffer_ += raw;
                    if (control_buffer_.size() > 256) {
                        fail("CSI sequence exceeds bounded input size");
                        return false;
                    }
                } else {
                    fail("invalid CSI byte in virtual display");
                    return false;
                }
                break;

            case ParseState::Dcs:
                if (byte == 0x1B) {
                    state_ = ParseState::DcsEscape;
                } else {
                    control_buffer_ += raw;
                    if (control_buffer_.size() > kMaxControlBytes) {
                        fail("DCS sequence exceeds bounded input size");
                        return false;
                    }
                }
                break;

            case ParseState::Osc:
                // Both terminators a host must accept: BEL, and ST written
                // as ESC \. BEL is a C0 byte that would be refused in
                // ground state, which is exactly why it is handled here.
                if (byte == 0x07) {
                    if (!handle_osc(control_buffer_)) return false;
                    control_buffer_.clear();
                    state_ = ParseState::Ground;
                } else if (byte == 0x1B) {
                    state_ = ParseState::OscEscape;
                } else if (byte < 0x20) {
                    fail("invalid control byte inside OSC sequence in virtual display");
                    return false;
                } else {
                    control_buffer_ += raw;
                    if (control_buffer_.size() > kMaxControlBytes) {
                        fail("OSC sequence exceeds bounded input size");
                        return false;
                    }
                }
                break;

            case ParseState::OscEscape:
                if (raw != '\\') {
                    fail("unsupported ESC inside OSC sequence");
                    return false;
                }
                if (!handle_osc(control_buffer_)) return false;
                control_buffer_.clear();
                state_ = ParseState::Ground;
                break;

            case ParseState::DcsEscape:
                if (raw != '\\') {
                    fail("unsupported ESC inside DCS sequence");
                    return false;
                }
                if (!decode_sixel(control_buffer_)) return false;
                control_buffer_.clear();
                state_ = ParseState::Ground;
                break;
        }
    }
    if (state_ == ParseState::Ground) return flush_text();
    return valid();
}

bool VirtualDisplay::finish() {
    if (!valid()) return false;
    if (state_ != ParseState::Ground) {
        fail("incomplete VT control sequence at write boundary");
        return false;
    }
    return flush_text();
}

bool VirtualDisplay::flush_text() {
    if (text_buffer_.empty()) return true;
    std::size_t pos = 0;
    while (pos < text_buffer_.size()) {
        const std::size_t end = text::grapheme_end(text_buffer_, pos);
        put_grapheme(std::string_view(text_buffer_).substr(pos, end - pos));
        pos = end;
    }
    text_buffer_.clear();
    return true;
}

bool VirtualDisplay::handle_osc(std::string_view body) {
    // OSC 22 sets the mouse pointer shape. Recorded rather than acted on:
    // this display has no pointer to draw, and what a headless verification
    // wants to know is which name the host was given.
    //
    // Every other OSC is refused, matching this parser's posture everywhere
    // else — it models exactly what ckVision emits, so an unmodelled
    // sequence means the emitter and the model have drifted apart, and
    // silently consuming it is how that goes unnoticed until a real host
    // disagrees.
    constexpr std::string_view kPointerShapePrefix = "22;";
    if (body == "22" || body.substr(0, kPointerShapePrefix.size()) == kPointerShapePrefix) {
        pointer_shape_name_ = body.size() > kPointerShapePrefix.size()
                                  ? std::string(body.substr(kPointerShapePrefix.size()))
                                  : std::string();
        return true;
    }
    fail("unsupported OSC sequence in virtual display");
    return false;
}

bool VirtualDisplay::handle_csi(char final_byte) {
    // DECSCUSR: CSI Ps SP q. Keep the deterministic decoder aware of the
    // cursor declaration emitted by Presenter so headless verification sees
    // the same visible/blinking cursor contract as a real host terminal.
    if (final_byte == 'q' && control_buffer_.size() >= 2 && control_buffer_.back() == ' ') {
        const std::string_view parameter_text{control_buffer_.data(), control_buffer_.size() - 1};
        bool ok = false;
        const std::vector<int> params = parse_csi_params(parameter_text, ok);
        if (!ok || params.size() != 1 || params[0] < 1 || params[0] > 6) {
            fail("invalid cursor-style sequence");
            return false;
        }
        const int style = params[0];
        cursor_.blink = (style & 1) != 0;
        cursor_.shape = style <= 2 ? CursorShape::Block : style <= 4 ? CursorShape::Underline : CursorShape::Bar;
        return true;
    }
    if (final_byte == 'H' || final_byte == 'f') {
        bool ok = false;
        const std::vector<int> params = parse_csi_params(control_buffer_, ok);
        if (!ok || params.size() > 2) {
            fail("invalid cursor-position CSI sequence");
            return false;
        }
        const int row = params.empty() || params[0] == 0 ? 1 : params[0];
        const int col = params.size() < 2 || params[1] == 0 ? 1 : params[1];
        cursor_.position = Point{std::max(0, col - 1), std::max(0, row - 1)};
        return true;
    }

    if (final_byte == 'm') {
        bool ok = false;
        std::vector<SgrParam> params = parse_sgr_params(control_buffer_, ok);
        if (!ok) {
            fail("invalid SGR CSI sequence");
            return false;
        }
        if (params.empty()) params.push_back(SgrParam{});
        return apply_sgr(params);
    }

    if (final_byte == 'J' || final_byte == 'K') {
        bool ok = false;
        const std::vector<int> params = parse_csi_params(control_buffer_, ok);
        if (!ok || params.size() > 1) {
            fail("invalid erase CSI sequence");
            return false;
        }
        const int mode = params.empty() ? 0 : params[0];
        if (mode < 0 || mode > (final_byte == 'J' ? 3 : 2)) {
            fail("unsupported erase CSI mode");
            return false;
        }
        if (final_byte == 'J') {
            if (mode == 0)
                erase_cells(cursor_.position.x, cursor_.position.y, size_.width, size_.height);
            else if (mode == 1)
                erase_cells(0, 0, cursor_.position.x + 1, cursor_.position.y + 1);
            else
                erase_cells(0, 0, size_.width, size_.height);
        } else if (mode == 0) {
            erase_cells(cursor_.position.x, cursor_.position.y, size_.width, cursor_.position.y + 1);
        } else if (mode == 1) {
            erase_cells(0, cursor_.position.y, cursor_.position.x + 1, cursor_.position.y + 1);
        } else {
            erase_cells(0, cursor_.position.y, size_.width, cursor_.position.y + 1);
        }
        return true;
    }

    if (final_byte == 'S' || final_byte == 'T') {
        bool ok = false;
        const std::vector<int> params = parse_csi_params(control_buffer_, ok);
        if (!ok || params.size() > 1) {
            fail("invalid scroll CSI sequence");
            return false;
        }
        const int count = params.empty() || params[0] == 0 ? 1 : params[0];
        if (count < 0) {
            fail("invalid scroll count");
            return false;
        }
        scroll_rows(final_byte == 'S' ? count : -count);
        return true;
    }

    if (final_byte == 'h' || final_byte == 'l') {
        if (control_buffer_.empty() || control_buffer_[0] != '?') {
            fail("unsupported non-private mode sequence");
            return false;
        }
        bool ok = false;
        const std::vector<int> params = parse_csi_params(std::string_view(control_buffer_).substr(1), ok);
        if (!ok || params.size() != 1 || (params[0] != 25 && params[0] != 2026)) {
            fail("unsupported private mode sequence");
            return false;
        }
        if (params[0] == 25) {
            cursor_.visible = (final_byte == 'h');
        } else if (final_byte == 'h') {
            begin_synchronized_output();
        } else {
            end_synchronized_output();
        }
        return true;
    }

    // A Device Status Report is a question, not a drawing operation: it
    // changes nothing on the screen this display models, and a real
    // terminal answers it on the input path this one does not have. It is
    // accepted rather than rejected because the Presenter writes it after a
    // frame to learn that the frame arrived (Presenter::
    // set_frame_completion_tracking), and a display that refuses to model
    // what the Presenter can emit cannot be used to test it.
    if (final_byte == 'n' && control_buffer_.find('?') == std::string::npos) return true;

    fail("unsupported CSI final byte in virtual display");
    return false;
}

bool VirtualDisplay::apply_sgr(const std::vector<SgrParam>& params) {
    // A palette index decodes back to a palette index. This decoder is an
    // oracle for what the Presenter wrote, and "the host was told index 4" is
    // the fact worth recording; resolving it to a particular blue here would
    // invent a palette the receiving terminal never consulted.
    const auto color_from = [](int mode, const int* values, std::size_t count,
                               Color& out) -> bool {
        if (mode == 5) {
            if (count < 1 || values[0] < 0 || values[0] > 255) return false;
            out = Color::indexed(static_cast<std::uint8_t>(values[0]));
            return true;
        }
        if (mode != 2 || count < 3) return false;
        for (std::size_t i = 0; i < 3; ++i)
            if (values[i] < 0 || values[i] > 255) return false;
        out = Color::rgb(static_cast<std::uint8_t>(values[0]), static_cast<std::uint8_t>(values[1]),
                         static_cast<std::uint8_t>(values[2]));
        return true;
    };

    std::size_t i = 0;
    while (i < params.size()) {
        const SgrParam& parameter = params[i++];
        const int p = parameter.value;
        // Only the parameters documented below carry sub-parameters; one
        // anywhere else is a form the Presenter cannot have produced.
        if (!parameter.subs.empty() && p != 4 && p != 58 && p != 38 && p != 48) {
            fail("unexpected SGR sub-parameter in virtual display");
            return false;
        }
        switch (p) {
            case 0:
                style_ = Style{};
                break;
            case 1:
                style_.attrs |= Attr::Bold;
                break;
            case 2:
                style_.attrs |= Attr::Dim;
                break;
            case 3:
                style_.attrs |= Attr::Italic;
                break;
            case 4: {
                // Bare `4` is the plain rule; `4:0` is no underline at all,
                // and the rest name a shape.
                const int shape = parameter.subs.empty() ? 1 : parameter.subs.front();
                if (parameter.subs.size() > 1 || shape < 0 || shape > 5) {
                    fail("unsupported underline style in virtual display");
                    return false;
                }
                if (shape == 0) {
                    style_.attrs = without_attr(style_.attrs, Attr::Underline);
                    style_.underline = UnderlineShape::Straight;
                    style_.underline_color = Color{};
                    break;
                }
                style_.attrs |= Attr::Underline;
                style_.underline = shape == 2   ? UnderlineShape::Double
                                   : shape == 3 ? UnderlineShape::Curly
                                   : shape == 4 ? UnderlineShape::Dotted
                                   : shape == 5 ? UnderlineShape::Dashed
                                                : UnderlineShape::Straight;
                break;
            }
            case 7:
                style_.attrs |= Attr::Reverse;
                break;
            case 9:
                style_.attrs |= Attr::Strike;
                break;
            case 22:
                style_.attrs = without_attr(without_attr(style_.attrs, Attr::Bold), Attr::Dim);
                break;
            case 23:
                style_.attrs = without_attr(style_.attrs, Attr::Italic);
                break;
            case 24:
                style_.attrs = without_attr(style_.attrs, Attr::Underline);
                style_.underline = UnderlineShape::Straight;
                style_.underline_color = Color{};
                break;
            case 27:
                style_.attrs = without_attr(style_.attrs, Attr::Reverse);
                break;
            case 29:
                style_.attrs = without_attr(style_.attrs, Attr::Strike);
                break;
            case 39:
                style_.fg = Color{};
                break;
            case 49:
                style_.bg = Color{};
                break;
            case 59:
                style_.underline_color = Color{};
                break;
            case 38:
            case 48:
            case 58: {
                Color color;
                if (!parameter.subs.empty()) {
                    // The ISO 8613-6 spelling, in which the whole colour is
                    // one parameter: `58:5:n`, or `58:2::R:G:B` with the
                    // colour space left unnamed.
                    const std::vector<int>& subs = parameter.subs;
                    const int mode = subs[0];
                    const std::size_t skip = (mode == 2 && subs.size() >= 5) ? 2 : 1;
                    if (!color_from(mode, subs.data() + skip, subs.size() - skip, color)) {
                        fail("invalid extended-color SGR sub-parameters");
                        return false;
                    }
                } else {
                    if (i >= params.size()) {
                        fail("truncated extended-color SGR sequence");
                        return false;
                    }
                    const int mode = params[i++].value;
                    const std::size_t count = mode == 2 ? 3 : 1;
                    if (params.size() - i < count) {
                        fail("truncated extended-color SGR sequence");
                        return false;
                    }
                    int values[3] = {0, 0, 0};
                    for (std::size_t v = 0; v < count; ++v) {
                        if (!params[i + v].subs.empty()) {
                            fail("unexpected SGR sub-parameter in virtual display");
                            return false;
                        }
                        values[v] = params[i + v].value;
                    }
                    i += count;
                    if (!color_from(mode, values, count, color)) {
                        fail("unsupported extended-color SGR mode");
                        return false;
                    }
                }
                if (p == 48) style_.bg = color;
                else if (p == 58) style_.underline_color = color;
                else style_.fg = color;
                break;
            }
            case 21:
                style_.attrs |= Attr::Underline;
                style_.underline = UnderlineShape::Double;
                break;
            default:
                if ((p >= 30 && p <= 37) || (p >= 90 && p <= 97)) {
                    style_.fg = Color::indexed(static_cast<std::uint8_t>(p >= 90 ? p - 90 + 8 : p - 30));
                } else if ((p >= 40 && p <= 47) || (p >= 100 && p <= 107)) {
                    style_.bg = Color::indexed(static_cast<std::uint8_t>(p >= 100 ? p - 100 + 8 : p - 40));
                } else {
                    fail("unsupported SGR parameter in virtual display");
                    return false;
                }
                break;
        }
    }
    return true;
}

void VirtualDisplay::put_grapheme(std::string_view grapheme) {
    const int width = text::grapheme_width(grapheme);
    int x = cursor_.position.x;
    const int y = cursor_.position.y;

    if (width == 0) {
        if (y >= 0 && y < size_.height && x > 0 && x <= size_.width) {
            int origin = x - 1;
            while (origin > 0 && cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width) +
                                        static_cast<std::size_t>(origin)]
                                     .is_continuation())
                --origin;
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width) +
                                      static_cast<std::size_t>(origin);
            std::string combined(cells_[index].grapheme());
            combined += grapheme;
            cells_[index] = Cell::from_grapheme(combined, style_);
        }
        return;
    }

    if (y >= 0 && y < size_.height && x >= 0 && x < size_.width) {
        const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width) +
                                  static_cast<std::size_t>(x);
        if (cells_[index].is_continuation() && x > 0) {
            cells_[index - 1] = Cell::from_grapheme(" ", style_);
        } else if (cells_[index].width() > 1 && x + 1 < size_.width) {
            cells_[index + 1] = Cell::from_grapheme(" ", style_);
        }

        cells_[index] = Cell::from_grapheme(grapheme, style_);
        const int stored_width = cells_[index].width();
        if (stored_width > 1 && x + 1 < size_.width) cells_[index + 1] = Cell::continuation(style_);
        clear_cell_pixels(x, y, std::max(1, stored_width));
    }
    cursor_.position.x += width;
}

void VirtualDisplay::clear_cell_pixels(int cell_x, int cell_y, int cell_width) noexcept {
    clear_pixel_rect(static_cast<std::int64_t>(cell_x) * cell_pixels_.width,
                     static_cast<std::int64_t>(cell_y) * cell_pixels_.height,
                     static_cast<std::int64_t>(cell_x + cell_width) * cell_pixels_.width,
                     static_cast<std::int64_t>(cell_y + 1) * cell_pixels_.height);
}

void VirtualDisplay::clear_pixel_rect(std::int64_t left, std::int64_t top, std::int64_t right,
                                      std::int64_t bottom) noexcept {
    const int start_x = static_cast<int>(
        std::clamp(left, std::int64_t{0}, static_cast<std::int64_t>(raster_plane_.width())));
    const int start_y = static_cast<int>(
        std::clamp(top, std::int64_t{0}, static_cast<std::int64_t>(raster_plane_.height())));
    const int end_x = static_cast<int>(
        std::clamp(right, std::int64_t{0}, static_cast<std::int64_t>(raster_plane_.width())));
    const int end_y = static_cast<int>(
        std::clamp(bottom, std::int64_t{0}, static_cast<std::int64_t>(raster_plane_.height())));
    for (int y = start_y; y < end_y; ++y)
        for (int x = start_x; x < end_x; ++x) raster_plane_.set_pixel(x, y, Image::Rgba{0, 0, 0, 0});
}

void VirtualDisplay::erase_cells(int left, int top, int right, int bottom) noexcept {
    left = std::clamp(left, 0, size_.width);
    top = std::clamp(top, 0, size_.height);
    right = std::clamp(right, 0, size_.width);
    bottom = std::clamp(bottom, 0, size_.height);
    if (left >= right || top >= bottom) return;

    for (int y = top; y < bottom; ++y) {
        int row_left = left;
        int row_right = right;
        if (row_left > 0 && cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width) +
                                    static_cast<std::size_t>(row_left)]
                                .is_continuation())
            --row_left;
        if (row_right < size_.width &&
            cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width) +
                   static_cast<std::size_t>(row_right)]
                .is_continuation())
            ++row_right;
        for (int x = row_left; x < row_right; ++x)
            cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width) +
                   static_cast<std::size_t>(x)] =
                Cell::from_grapheme(" ", style_);
        clear_cell_pixels(row_left, y, row_right - row_left);
    }
}

void VirtualDisplay::scroll_rows(int rows) noexcept {
    if (rows == 0 || size_.height == 0) return;
    const int shift = std::min(std::abs(rows), size_.height);
    std::vector<Cell> next(static_cast<std::size_t>(size_.width) * static_cast<std::size_t>(size_.height),
                           Cell::from_grapheme(" ", style_));
    Image next_raster(raster_plane_.width(), raster_plane_.height());
    const int pixel_shift = shift * cell_pixels_.height;
    for (int y = 0; y < size_.height; ++y) {
        const int source_y = rows > 0 ? y + shift : y - shift;
        if (source_y < 0 || source_y >= size_.height) continue;
        for (int x = 0; x < size_.width; ++x)
            next[static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width) +
                 static_cast<std::size_t>(x)] =
                cells_[static_cast<std::size_t>(source_y) * static_cast<std::size_t>(size_.width) +
                       static_cast<std::size_t>(x)];
        const int source_pixel_y = rows > 0 ? y * cell_pixels_.height + pixel_shift
                                            : y * cell_pixels_.height - pixel_shift;
        if (source_pixel_y < 0 || source_pixel_y + cell_pixels_.height > raster_plane_.height()) continue;
        for (int py = 0; py < cell_pixels_.height; ++py)
            for (int px = 0; px < raster_plane_.width(); ++px)
                next_raster.set_pixel(px, y * cell_pixels_.height + py,
                                      raster_plane_.pixel(px, source_pixel_y + py));
    }
    cells_ = std::move(next);
    raster_plane_ = std::move(next_raster);
}

bool VirtualDisplay::decode_sixel(std::string_view body) {
    // The picture is decoded on its own terms and then blitted here. This
    // display used to read the Sixel grammar itself, straight into its plane;
    // the emulator that hosts child graphics then had to build a whole
    // display, the size of the terminal, to borrow that reading for one
    // picture. One decoder, at the size of the picture, serves both.
    std::string error;
    const std::int64_t start_x = static_cast<std::int64_t>(cursor_.position.x) * cell_pixels_.width;
    const std::int64_t start_y = static_cast<std::int64_t>(cursor_.position.y) * cell_pixels_.height;
    const Size room{static_cast<int>(std::clamp<std::int64_t>(raster_plane_.width() - start_x, 0,
                                                             raster_plane_.width())),
                    static_cast<int>(std::clamp<std::int64_t>(raster_plane_.height() - start_y, 0,
                                                              raster_plane_.height()))};
    const std::size_t plane_pixels = static_cast<std::size_t>(std::max(1, raster_plane_.width())) *
                                     static_cast<std::size_t>(std::max(1, raster_plane_.height()));
    const std::optional<DecodedSixel> decoded =
        ckv::term::decode_sixel(body, room, plane_pixels, sixel_palette_, error);
    if (!decoded) {
        fail(error);
        return false;
    }

    const std::int64_t origin_x = start_x;
    const std::int64_t origin_y = start_y;
    // P2 = 0 with a declared raster size clears that whole rectangle first,
    // including the part of it the picture leaves untouched.
    if (decoded->erases_background && decoded->declared.width > 0 && decoded->declared.height > 0)
        clear_pixel_rect(origin_x, origin_y, origin_x + decoded->declared.width,
                         origin_y + decoded->declared.height);

    const Image& picture = decoded->image;
    for (int y = 0; y < picture.height(); ++y) {
        const std::int64_t py = origin_y + y;
        if (py < 0 || py >= raster_plane_.height()) continue;
        for (int x = 0; x < picture.width(); ++x) {
            const std::int64_t px = origin_x + x;
            if (px < 0 || px >= raster_plane_.width()) continue;
            const Image::Rgba pixel = picture.pixel(x, y);
            // Transparent means the sequence never drew here, and what was
            // under it stays — a Sixel paints, it does not blank.
            if (pixel.a == 0) continue;
            raster_plane_.set_pixel(static_cast<int>(px), static_cast<int>(py), pixel);
        }
    }
    return true;
}

void VirtualDisplay::resize(Size cells) {
    size_ = Size{std::max(0, cells.width), std::max(0, cells.height)};
    discard_synchronized_snapshot();
    clear();
}

void VirtualDisplay::set_cell_pixels(Size cell_pixels) {
    const Size normalized{std::max(1, cell_pixels.width), std::max(1, cell_pixels.height)};
    if (normalized == cell_pixels_) return;
    discard_synchronized_snapshot();
    cell_pixels_ = normalized;
    raster_plane_ = Image(size_.width * cell_pixels_.width, size_.height * cell_pixels_.height);
}

void VirtualDisplay::clear() {
    cells_.assign(static_cast<std::size_t>(size_.width) * static_cast<std::size_t>(size_.height), Cell{});
    raster_plane_ = Image(size_.width * cell_pixels_.width, size_.height * cell_pixels_.height);
    cursor_ = CursorState{};
    cursor_.position = Point{0, 0};
    style_ = Style{};
    state_ = ParseState::Ground;
    text_buffer_.clear();
    control_buffer_.clear();
    error_.clear();
    sixel_palette_ = SixelPalette{};
    discard_synchronized_snapshot();
}

void VirtualDisplay::begin_synchronized_output() {
    if (synchronized_output_) return;
    visible_cells_ = cells_;
    visible_raster_plane_ = raster_plane_;
    visible_cursor_ = cursor_;
    synchronized_output_ = true;
}

void VirtualDisplay::end_synchronized_output() noexcept {
    if (!synchronized_output_) return;
    discard_synchronized_snapshot();
}

void VirtualDisplay::discard_synchronized_snapshot() noexcept {
    synchronized_output_ = false;
    visible_cells_.clear();
    visible_raster_plane_ = Image{};
    visible_cursor_ = CursorState{};
}

}  // namespace ckv::term
