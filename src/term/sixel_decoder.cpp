// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/sixel_decoder.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>

namespace ckv::term {
namespace {

constexpr int kMaxSixelRepeat = 1'000'000;

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

int advance_saturated(int value, int delta) noexcept {
    if (delta > 0 && value > std::numeric_limits<int>::max() - delta) return std::numeric_limits<int>::max();
    return value + delta;
}

int highest_set_row(unsigned char bits) noexcept {
    for (int bit = 5; bit >= 0; --bit)
        if ((bits & (1U << bit)) != 0) return bit;
    return -1;
}

// The grammar, once. Both passes — measuring the picture and drawing it —
// walk the sequence through this, so there is no second reading of Sixel in
// this file to drift from the first. `plot(x, y, repeat, bits, color)` is the
// only thing they do differently.
template <typename Plot>
bool walk_sixel(std::string_view body, SixelPalette& palette, std::string& error, bool& erases_background,
                int& declared_width, int& declared_height, Plot&& plot) {
    const std::size_t q = body.find('q');
    if (q == std::string_view::npos) {
        error = "DCS is not a Sixel sequence";
        return false;
    }
    int params[3] = {0, 0, 0};
    int param_count = 0;
    {
        std::size_t pos = 0;
        while (pos < q) {
            if (body[pos] == ';') {
                if (++param_count > 2) {
                    error = "invalid Sixel DCS parameters";
                    return false;
                }
                ++pos;
                continue;
            }
            if (!std::isdigit(static_cast<unsigned char>(body[pos]))) {
                error = "unsupported Sixel DCS parameter byte";
                return false;
            }
            if (!parse_unsigned(body, pos, params[param_count])) {
                error = "invalid Sixel DCS parameters";
                return false;
            }
        }
        if (param_count > 0 || q > 0) ++param_count;
    }
    const int background_mode = param_count > 1 ? params[1] : 0;
    if (background_mode != 0 && background_mode != 1) {
        error = "unsupported Sixel background mode";
        return false;
    }
    erases_background = background_mode == 0;

    int x = 0;
    int y = 0;
    int selected_color = 0;
    std::size_t pos = q + 1;
    while (pos < body.size()) {
        const unsigned char c = static_cast<unsigned char>(body[pos]);
        if (c >= 63 && c <= 126) {
            const auto bits = static_cast<unsigned char>(c - 63);
            plot(x, y, 1, bits, palette.colors[static_cast<std::size_t>(selected_color)]);
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
            if (!parse_unsigned(body, pos, index) || index < 0 || index >= kSixelColorRegisters) {
                error = "invalid Sixel color register";
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
                    !parse_unsigned(body, pos, b) || mode != 2 || r < 0 || r > 100 || g < 0 || g > 100 ||
                    b < 0 || b > 100) {
                    error = "unsupported or invalid Sixel color definition";
                    return false;
                }
                palette.colors[static_cast<std::size_t>(index)] =
                    Image::Rgba{static_cast<std::uint8_t>((r * 255 + 50) / 100),
                                static_cast<std::uint8_t>((g * 255 + 50) / 100),
                                static_cast<std::uint8_t>((b * 255 + 50) / 100), 255};
                palette.defined[static_cast<std::size_t>(index)] = true;
            } else if (!palette.defined[static_cast<std::size_t>(index)]) {
                palette.colors[static_cast<std::size_t>(index)] = Image::Rgba{0, 0, 0, 255};
            }
            continue;
        }
        if (c == '!') {
            ++pos;
            int repeat = 0;
            if (!parse_unsigned(body, pos, repeat) || repeat <= 0 || repeat > kMaxSixelRepeat ||
                pos >= body.size()) {
                error = "invalid or excessive Sixel repeat introducer";
                return false;
            }
            const auto data = static_cast<unsigned char>(body[pos++]);
            if (data < 63 || data > 126) {
                error = "Sixel repeat target is not a data byte";
                return false;
            }
            const auto bits = static_cast<unsigned char>(data - 63);
            plot(x, y, repeat, bits, palette.colors[static_cast<std::size_t>(selected_color)]);
            x = advance_saturated(x, repeat);
            continue;
        }
        if (c == '"') {
            ++pos;
            std::array<int, 4> fields{};
            int field_count = 0;
            while (pos < body.size() && field_count < 4) {
                if (!parse_unsigned(body, pos, fields[static_cast<std::size_t>(field_count)])) {
                    error = "invalid Sixel raster attribute";
                    return false;
                }
                ++field_count;
                if (pos >= body.size() || body[pos] != ';') break;
                ++pos;
            }
            if (field_count == 4) {
                if (fields[2] <= 0 || fields[3] <= 0) {
                    error = "invalid Sixel raster dimensions";
                    return false;
                }
                declared_width = fields[2];
                declared_height = fields[3];
            }
            continue;
        }

        error = "unsupported byte in Sixel payload";
        return false;
    }
    return true;
}

}  // namespace

std::optional<DecodedSixel> decode_sixel(std::string_view body, Size visible, std::size_t max_pixels,
                                         SixelPalette& palette, std::string& error) {
    // Measuring first. The palette is walked on a copy so that a sequence
    // rejected for its size leaves the terminal's registers as they were.
    SixelPalette probe = palette;
    bool erases_background = true;
    int declared_width = 0;
    int declared_height = 0;
    int drawn_width = 0;
    int drawn_height = 0;
    const auto measure = [&](int x, int y, int repeat, unsigned char bits, Image::Rgba) {
        const int row = highest_set_row(bits);
        if (row < 0 || repeat <= 0) return;
        drawn_width = std::max(drawn_width, advance_saturated(x, repeat));
        drawn_height = std::max(drawn_height, y + row + 1);
    };
    if (!walk_sixel(body, probe, error, erases_background, declared_width, declared_height, measure))
        return std::nullopt;

    const int drew_width = std::max(declared_width, drawn_width);
    const int drew_height = std::max(declared_height, drawn_height);
    if (drew_width <= 0 || drew_height <= 0) {
        error = "Sixel sequence contained no visible pixels";
        return std::nullopt;
    }
    // Cut off at the edge of the room it has. What is past the edge cannot be
    // seen and cannot be scrolled into view — a picture is anchored to the
    // cell it started on — so keeping it would be paying to store the
    // invisible, which is how a one-million-pixel-wide repeat turns into a
    // refusal instead of the sliver a terminal would show.
    const int width = std::min(drew_width, std::max(0, visible.width));
    const int height = std::min(drew_height, std::max(0, visible.height));
    if (width <= 0 || height <= 0)
        return DecodedSixel{Image{}, erases_background, Size{declared_width, declared_height}};
    if (static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) > max_pixels) {
        error = "Sixel picture of " + std::to_string(width) + "x" + std::to_string(height) +
                " pixels is past max_image_pixels (" + std::to_string(max_pixels) + ")";
        return std::nullopt;
    }

    DecodedSixel decoded{Image(width, height), erases_background, Size{declared_width, declared_height}};
    Image& image = decoded.image;
    const auto draw = [&](int x, int y, int repeat, unsigned char bits, Image::Rgba color) {
        if (repeat <= 0 || bits == 0) return;
        const int end = std::min(width, advance_saturated(x, repeat));
        for (int bit = 0; bit < 6; ++bit) {
            if ((bits & (1U << bit)) == 0) continue;
            const int py = y + bit;
            if (py < 0 || py >= height) continue;
            for (int px = std::max(0, x); px < end; ++px) image.set_pixel(px, py, color);
        }
    };
    // The measuring pass proved the grammar; this one cannot fail on it, and
    // it is the pass whose palette edits the terminal keeps.
    if (!walk_sixel(body, palette, error, erases_background, declared_width, declared_height, draw))
        return std::nullopt;
    return decoded;
}

}  // namespace ckv::term
