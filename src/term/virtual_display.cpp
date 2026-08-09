// Copyright (c) 2026 C. Klukas. All rights reserved.
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

namespace ckv::term {
namespace {

constexpr std::size_t kMaxControlBytes = 16U * 1024U * 1024U;
constexpr int kMaxSixelRepeat = 1'000'000;

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

constexpr std::array<Rgb, 16> kBasic16{{
    {0, 0, 0},       {128, 0, 0},     {0, 128, 0},     {128, 128, 0},
    {0, 0, 128},     {128, 0, 128},   {0, 128, 128},   {192, 192, 192},
    {128, 128, 128}, {255, 0, 0},     {0, 255, 0},     {255, 255, 0},
    {0, 0, 255},     {255, 0, 255},   {0, 255, 255},   {255, 255, 255},
}};

Rgb indexed_color(int index) noexcept {
    if (index < 0) index = 0;
    if (index < 16) return kBasic16[static_cast<std::size_t>(index)];
    if (index < 232) {
        constexpr std::array<std::uint8_t, 6> levels{{0, 95, 135, 175, 215, 255}};
        const int cube = index - 16;
        return Rgb{levels[static_cast<std::size_t>((cube / 36) % 6)],
                   levels[static_cast<std::size_t>((cube / 6) % 6)],
                   levels[static_cast<std::size_t>(cube % 6)]};
    }
    if (index < 256) {
        const auto value = static_cast<std::uint8_t>(8 + (index - 232) * 10);
        return Rgb{value, value, value};
    }
    return kBasic16[0];
}

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

Attr without_attr(Attr attrs, Attr removed) noexcept {
    const auto bits = static_cast<std::uint8_t>(attrs);
    const auto mask = static_cast<std::uint8_t>(removed);
    return static_cast<Attr>(bits & static_cast<std::uint8_t>(~mask));
}

int advance_saturated(int value, int delta) noexcept {
    if (delta > 0 && value > std::numeric_limits<int>::max() - delta) {
        return std::numeric_limits<int>::max();
    }
    return value + delta;
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

bool VirtualDisplay::handle_csi(char final_byte) {
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
        std::vector<int> params = parse_csi_params(control_buffer_, ok);
        if (!ok) {
            fail("invalid SGR CSI sequence");
            return false;
        }
        if (params.empty()) params.push_back(0);
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

    fail("unsupported CSI final byte in virtual display");
    return false;
}

bool VirtualDisplay::apply_sgr(const std::vector<int>& params) {
    std::size_t i = 0;
    while (i < params.size()) {
        const int p = params[i++];
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
            case 4:
                style_.attrs |= Attr::Underline;
                break;
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
            case 38:
            case 48: {
                const bool background = p == 48;
                if (i >= params.size()) {
                    fail("truncated extended-color SGR sequence");
                    return false;
                }
                Color color;
                const int mode = params[i++];
                if (mode == 2) {
                    if (i + 2 >= params.size()) {
                        fail("truncated truecolor SGR sequence");
                        return false;
                    }
                    const int r = params[i++];
                    const int g = params[i++];
                    const int b = params[i++];
                    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
                        fail("out-of-range truecolor SGR component");
                        return false;
                    }
                    color = Color::rgb(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                                       static_cast<std::uint8_t>(b));
                } else if (mode == 5) {
                    if (i >= params.size() || params[i] < 0 || params[i] > 255) {
                        fail("invalid indexed-color SGR sequence");
                        return false;
                    }
                    const Rgb rgb = indexed_color(params[i++]);
                    color = Color::rgb(rgb.r, rgb.g, rgb.b);
                } else {
                    fail("unsupported extended-color SGR mode");
                    return false;
                }
                if (background)
                    style_.bg = color;
                else
                    style_.fg = color;
                break;
            }
            default:
                if ((p >= 30 && p <= 37) || (p >= 90 && p <= 97)) {
                    const int index = p >= 90 ? p - 90 + 8 : p - 30;
                    const Rgb rgb = indexed_color(index);
                    style_.fg = Color::rgb(rgb.r, rgb.g, rgb.b);
                } else if ((p >= 40 && p <= 47) || (p >= 100 && p <= 107)) {
                    const int index = p >= 100 ? p - 100 + 8 : p - 40;
                    const Rgb rgb = indexed_color(index);
                    style_.bg = Color::rgb(rgb.r, rgb.g, rgb.b);
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

void VirtualDisplay::plot_sixel_run(int x, int y, int repeat, unsigned char bits, Rgba color) noexcept {
    if (repeat <= 0 || bits == 0) return;

    const std::int64_t origin_x = static_cast<std::int64_t>(cursor_.position.x) * cell_pixels_.width + x;
    const std::int64_t origin_y = static_cast<std::int64_t>(cursor_.position.y) * cell_pixels_.height + y;
    const std::int64_t plane_width = raster_plane_.width();
    const std::int64_t plane_height = raster_plane_.height();
    const std::int64_t first_column = std::max(std::int64_t{0}, -origin_x);
    const std::int64_t end_column = std::min(static_cast<std::int64_t>(repeat), plane_width - origin_x);
    if (first_column >= end_column) return;

    for (int bit = 0; bit < 6; ++bit) {
        if ((bits & (1U << bit)) == 0) continue;
        const std::int64_t py = origin_y + bit;
        if (py < 0 || py >= plane_height) continue;
        for (std::int64_t column = first_column; column < end_column; ++column) {
            const std::int64_t px = origin_x + column;
            raster_plane_.set_pixel(static_cast<int>(px), static_cast<int>(py),
                                    Image::Rgba{color.r, color.g, color.b, color.a});
        }
    }
}

bool VirtualDisplay::decode_sixel(std::string_view body) {
    const std::size_t q = body.find('q');
    if (q == std::string_view::npos) {
        fail("DCS is not a Sixel sequence");
        return false;
    }
    for (std::size_t i = 0; i < q; ++i) {
        const char c = body[i];
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != ';') {
            fail("unsupported Sixel DCS parameter byte");
            return false;
        }
    }

    bool params_ok = false;
    const std::vector<int> dcs_params = parse_csi_params(body.substr(0, q), params_ok);
    if (!params_ok || dcs_params.size() > 3) {
        fail("invalid Sixel DCS parameters");
        return false;
    }
    const int background_mode = dcs_params.size() > 1 ? dcs_params[1] : 0;
    if (background_mode != 0 && background_mode != 1) {
        fail("unsupported Sixel background mode");
        return false;
    }

    int x = 0;
    int y = 0;
    int selected_color = 0;
    int raster_width = 0;
    int raster_height = 0;
    std::size_t pos = q + 1;
    while (pos < body.size()) {
        const unsigned char c = static_cast<unsigned char>(body[pos]);
        if (c >= 63 && c <= 126) {
            const unsigned char bits = static_cast<unsigned char>(c - 63);
            const Rgba color = sixel_palette_[static_cast<std::size_t>(selected_color)];
            plot_sixel_run(x, y, 1, bits, color);
            x = advance_saturated(x, 1);
            ++pos;
            continue;
        }
        if (c == '$') {
            x = 0;
            ++pos;
            continue;
        }
        if (c == '-') {
            x = 0;
            y += 6;
            ++pos;
            continue;
        }
        if (c == '#') {
            ++pos;
            int index = 0;
            if (!parse_unsigned(body, pos, index) || index < 0 || index >= 256) {
                fail("invalid Sixel color register");
                return false;
            }
            selected_color = index;
            if (pos < body.size() && body[pos] == ';') {
                ++pos;
                int mode = 0;
                int r = 0;
                int g = 0;
                int b = 0;
                if (!parse_unsigned(body, pos, mode) || pos >= body.size() || body[pos++] != ';' ||
                    !parse_unsigned(body, pos, r) || pos >= body.size() || body[pos++] != ';' ||
                    !parse_unsigned(body, pos, g) || pos >= body.size() || body[pos++] != ';' ||
                    !parse_unsigned(body, pos, b) || mode != 2 || r < 0 || r > 100 || g < 0 ||
                    g > 100 || b < 0 || b > 100) {
                    fail("unsupported or invalid Sixel color definition");
                    return false;
                }
                sixel_palette_[static_cast<std::size_t>(index)] =
                    Rgba{static_cast<std::uint8_t>((r * 255 + 50) / 100),
                         static_cast<std::uint8_t>((g * 255 + 50) / 100),
                         static_cast<std::uint8_t>((b * 255 + 50) / 100), 255};
                sixel_palette_defined_[static_cast<std::size_t>(index)] = true;
            } else if (!sixel_palette_defined_[static_cast<std::size_t>(index)]) {
                sixel_palette_[static_cast<std::size_t>(index)] = Rgba{0, 0, 0, 255};
            }
            continue;
        }
        if (c == '!') {
            ++pos;
            int repeat = 0;
            if (!parse_unsigned(body, pos, repeat) || repeat <= 0 || repeat > kMaxSixelRepeat ||
                pos >= body.size()) {
                fail("invalid or excessive Sixel repeat introducer");
                return false;
            }
            const unsigned char data = static_cast<unsigned char>(body[pos++]);
            if (data < 63 || data > 126) {
                fail("Sixel repeat target is not a data byte");
                return false;
            }
            const unsigned char bits = static_cast<unsigned char>(data - 63);
            const Rgba color = sixel_palette_[static_cast<std::size_t>(selected_color)];
            plot_sixel_run(x, y, repeat, bits, color);
            x = advance_saturated(x, repeat);
            continue;
        }
        if (c == '"') {
            ++pos;
            std::array<int, 4> fields{};
            int field_count = 0;
            while (pos < body.size() && field_count < 4) {
                if (!parse_unsigned(body, pos, fields[static_cast<std::size_t>(field_count)])) {
                    fail("invalid Sixel raster attribute");
                    return false;
                }
                ++field_count;
                if (pos >= body.size() || body[pos] != ';') break;
                ++pos;
            }
            if (field_count == 4) {
                raster_width = fields[2];
                raster_height = fields[3];
                if (raster_width <= 0 || raster_height <= 0) {
                    fail("invalid Sixel raster dimensions");
                    return false;
                }
                if (background_mode == 0)
                    clear_pixel_rect(static_cast<std::int64_t>(cursor_.position.x) * cell_pixels_.width,
                                     static_cast<std::int64_t>(cursor_.position.y) * cell_pixels_.height,
                                     static_cast<std::int64_t>(cursor_.position.x) * cell_pixels_.width +
                                         raster_width,
                                     static_cast<std::int64_t>(cursor_.position.y) * cell_pixels_.height +
                                         raster_height);
            }
            continue;
        }

        fail("unsupported byte in Sixel payload");
        return false;
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
    sixel_palette_.fill(Rgba{});
    sixel_palette_defined_.fill(false);
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
