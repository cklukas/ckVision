// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/terminal_emulator.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <initializer_list>
#include <limits>

#include "cvision/core/text.hpp"
#include "cvision/term/virtual_display.hpp"

namespace ckv::term {
namespace {
Size bounded_size(Size size, Size limit) noexcept {
    return {std::clamp(size.width, 1, std::max(1, limit.width)),
            std::clamp(size.height, 1, std::max(1, limit.height))};
}

int ceil_div_positive(int value, int divisor) noexcept {
    if (value <= 0) return 0;
    return 1 + (value - 1) / std::max(1, divisor);
}

std::int64_t saturated_product(std::initializer_list<std::int64_t> factors) noexcept {
    constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    std::int64_t result = 1;
    for (const std::int64_t factor : factors) {
        if (factor <= 0) return 0;
        if (result > maximum / factor) return maximum;
        result *= factor;
    }
    return result;
}

Color ansi_color(int index) noexcept {
    constexpr std::array<Color, 16> colors{
        Color::rgb(0, 0, 0),       Color::rgb(205, 49, 49),   Color::rgb(13, 188, 121), Color::rgb(229, 229, 16),
        Color::rgb(36, 114, 200),  Color::rgb(188, 63, 188),  Color::rgb(17, 168, 205), Color::rgb(229, 229, 229),
        Color::rgb(102, 102, 102), Color::rgb(241, 76, 76),   Color::rgb(35, 209, 139), Color::rgb(245, 245, 67),
        Color::rgb(59, 142, 234),  Color::rgb(214, 112, 214), Color::rgb(41, 184, 219), Color::rgb(255, 255, 255),
    };
    if (index < 16) return colors[static_cast<std::size_t>(std::max(0, index))];
    if (index < 232) {
        constexpr std::array<std::uint8_t, 6> levels{{0, 95, 135, 175, 215, 255}};
        const int cube = index - 16;
        return Color::rgb(levels[static_cast<std::size_t>((cube / 36) % 6)],
                          levels[static_cast<std::size_t>((cube / 6) % 6)],
                          levels[static_cast<std::size_t>(cube % 6)]);
    }
    if (index < 256) {
        const auto value = static_cast<std::uint8_t>(8 + (index - 232) * 10);
        return Color::rgb(value, value, value);
    }
    return colors[0];
}

std::size_t complete_utf8_prefix(std::string_view text) noexcept {
    if (text.empty()) return 0;
    std::size_t start = text.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) --start;
    const unsigned char lead = static_cast<unsigned char>(text[start]);
    int expected = 1;
    if ((lead & 0xE0U) == 0xC0U) expected = 2;
    else if ((lead & 0xF0U) == 0xE0U) expected = 3;
    else if ((lead & 0xF8U) == 0xF0U) expected = 4;
    if (expected > 1 && text.size() - start < static_cast<std::size_t>(expected)) return start;
    return text.size();
}

struct CroppedRaster {
    std::shared_ptr<const Image> image;
    Point cell_offset;
    Size cell_extent;
};

std::optional<CroppedRaster> crop_visible_raster(const Image& source, Size cell_pixels) {
    const int cell_width = std::max(1, cell_pixels.width);
    const int cell_height = std::max(1, cell_pixels.height);
    int left = source.width();
    int top = source.height();
    int right = 0;
    int bottom = 0;
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            if (source.pixel(x, y).a == 0) continue;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }
    if (right <= left || bottom <= top) return std::nullopt;

    const int clipped_left = left / cell_width * cell_width;
    const int clipped_top = top / cell_height * cell_height;
    const int clipped_right = std::min(source.width(), (right + cell_width - 1) / cell_width * cell_width);
    const int clipped_bottom = std::min(source.height(), (bottom + cell_height - 1) / cell_height * cell_height);
    Image cropped(clipped_right - clipped_left, clipped_bottom - clipped_top);
    for (int y = clipped_top; y < clipped_bottom; ++y)
        for (int x = clipped_left; x < clipped_right; ++x)
            cropped.set_pixel(x - clipped_left, y - clipped_top, source.pixel(x, y));
    return CroppedRaster{std::make_shared<Image>(std::move(cropped)),
                         Point{clipped_left / cell_width, clipped_top / cell_height},
                         Size{(clipped_right - clipped_left + cell_width - 1) / cell_width,
                              (clipped_bottom - clipped_top + cell_height - 1) / cell_height}};
}

bool is_sixel_control(std::string_view control) noexcept {
    const std::size_t command = control.find('q');
    if (command == std::string_view::npos) return false;
    for (std::size_t i = 0; i < command; ++i)
        if (control[i] != ';' && (control[i] < '0' || control[i] > '9')) return false;
    return true;
}

}  // namespace

TerminalEmulator::TerminalEmulator(TerminalCapabilityProfile profile, TerminalSubsessionOptions options)
    : profile_(std::move(profile)), options_(std::move(options)), cells_(bounded_size(profile_.cells, options_.max_cells)),
      style_(profile_.default_style) {
    profile_.cells = cells_;
    primary_.assign(static_cast<std::size_t>(cells_.width * cells_.height), Cell::from_grapheme(" ", style_));
    alternate_ = primary_;
    scroll_bottom_ = cells_.height;
}

std::vector<Cell>& TerminalEmulator::active_cells() noexcept { return alternate_buffer_ ? alternate_ : primary_; }
const std::vector<Cell>& TerminalEmulator::active_cells() const noexcept { return alternate_buffer_ ? alternate_ : primary_; }

TerminalSnapshot TerminalEmulator::snapshot() const {
    const TerminalMouseEncoding mouse_encoding = !mouse_reporting_enabled_ ? TerminalMouseEncoding::None
        : mouse_sgr_enabled_ ? TerminalMouseEncoding::Sgr : TerminalMouseEncoding::X10;
    return TerminalSnapshot{cells_, active_cells(), cursor_, alternate_buffer_, rasters_, diagnostics_, title_, state_, scrollback_,
                            bracketed_paste_enabled_, mouse_reporting_enabled_, mouse_encoding,
                            application_cursor_keys_, focus_reporting_enabled_};
}

void TerminalEmulator::diagnostic(TerminalDiagnostic::Kind kind, std::string message) {
    diagnostics_.push_back(TerminalDiagnostic{kind, std::move(message)});
    if (diagnostics_.size() > 64) diagnostics_.erase(diagnostics_.begin());
}

bool TerminalEmulator::append_control(char byte) {
    if (control_.size() < options_.max_control_bytes) {
        control_.push_back(byte);
        return true;
    }
    diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child control string exceeded configured limit");
    control_.clear();
    parse_state_ = ParseState::Discard;
    return false;
}

bool TerminalEmulator::append_dcs_byte(char byte) {
    const std::size_t limit = dcs_sixel_payload_ ? options_.max_graphics_payload_bytes : options_.max_control_bytes;
    if (control_.size() < std::max<std::size_t>(1, limit)) {
        control_.push_back(byte);
        if (!dcs_sixel_payload_ && byte == 'q' && is_sixel_control(control_)) dcs_sixel_payload_ = true;
        return true;
    }
    diagnostic(TerminalDiagnostic::Kind::LimitExceeded,
               dcs_sixel_payload_ ? "child Sixel payload exceeded configured limit"
                                   : "child control string exceeded configured limit");
    control_.clear();
    dcs_sixel_payload_ = false;
    parse_state_ = ParseState::Discard;
    return false;
}

void TerminalEmulator::feed_output(std::string_view bytes) {
    if (state_ == TerminalSubsessionState::Closed || state_ == TerminalSubsessionState::Failed ||
        state_ == TerminalSubsessionState::Exited)
        return;
    state_ = TerminalSubsessionState::Running;
    const std::size_t output_limit = std::max<std::size_t>(1, options_.max_output_bytes);
    const std::size_t available = output_limit > pending_output_.size() ? output_limit - pending_output_.size() : 0;
    const std::size_t accepted = std::min(bytes.size(), available);
    pending_output_.append(bytes.substr(0, accepted));
    if (accepted < bytes.size())
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child output queue exceeded configured limit");

    const std::size_t parser_budget = std::max<std::size_t>(1, options_.max_parser_work_per_step);
    const std::size_t budget = std::min(pending_output_.size(), parser_budget);
    if (pending_output_.size() > budget)
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child output step exceeded parser work budget");
    for (std::size_t i = 0; i < budget; ++i) {
        const char byte = pending_output_[i];
        switch (parse_state_) {
            case ParseState::Ground:
                if (byte == '\x1b') { put_text(true); parse_state_ = ParseState::Escape; }
                else if (byte == '\r') { put_text(true); cursor_.position.x = 0; }
                else if (byte == '\n') { put_text(true); cursor_.position.x = 0; index(); }
                else if (byte == '\t') { put_text(true); cursor_.position.x = std::min(cells_.width, ((cursor_.position.x / 8) + 1) * 8); }
                else if (byte == '\b') { put_text(true); cursor_.position.x = std::max(0, cursor_.position.x - 1); }
                else if (static_cast<unsigned char>(byte) >= 0x20) {
                    if (printable_.size() == options_.max_printable_run_bytes) { diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child printable run exceeded configured limit"); put_text(); }
                    printable_.push_back(byte);
                }
                break;
            case ParseState::Escape:
                if (byte == '[') { control_.clear(); parse_state_ = ParseState::Csi; }
                else if (byte == ']') { control_.clear(); parse_state_ = ParseState::Osc; }
                else if (byte == 'P') { control_.clear(); dcs_sixel_payload_ = false; parse_state_ = ParseState::Dcs; }
                else if (byte == 'D') { index(); parse_state_ = ParseState::Ground; }
                else if (byte == 'E') { cursor_.position.x = 0; index(); parse_state_ = ParseState::Ground; }
                else if (byte == 'M') { reverse_index(); parse_state_ = ParseState::Ground; }
                else if (byte == '7') { saved_cursor_ = cursor_; saved_cursor_valid_ = true; parse_state_ = ParseState::Ground; }
                else if (byte == '8') { if (saved_cursor_valid_) cursor_ = saved_cursor_; parse_state_ = ParseState::Ground; }
                else { diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child escape sequence"); parse_state_ = ParseState::Ground; }
                break;
            case ParseState::Csi:
                if (byte >= '@' && byte <= '~') { handle_csi(byte); control_.clear(); parse_state_ = ParseState::Ground; }
                else append_control(byte);
                break;
            case ParseState::Osc:
                if (byte == '\a') { if (profile_.osc_policy == TerminalOscPolicy::StoreMetadata && control_.rfind("0;", 0) == 0) title_ = control_.substr(2); control_.clear(); parse_state_ = ParseState::Ground; }
                else if (byte == '\x1b') parse_state_ = ParseState::OscEscape;
                else append_control(byte);
                break;
            case ParseState::OscEscape:
                if (byte == '\\') { if (profile_.osc_policy == TerminalOscPolicy::StoreMetadata && control_.rfind("0;", 0) == 0) title_ = control_.substr(2); control_.clear(); parse_state_ = ParseState::Ground; }
                else {
                    const bool retained_escape = append_control('\x1b');
                    const bool retained_byte = retained_escape && append_control(byte);
                    if (retained_byte) parse_state_ = ParseState::Osc;
                }
                break;
            case ParseState::Dcs:
                if (byte == '\x1b') parse_state_ = ParseState::DcsEscape;
                else append_dcs_byte(byte);
                break;
            case ParseState::DcsEscape:
                if (byte == '\\') {
                    if (profile_.sixel && is_sixel_control(control_)) {
                        const std::int64_t pixels = saturated_product({
                            cells_.width, cells_.height, std::max(1, profile_.cell_pixels.width),
                            std::max(1, profile_.cell_pixels.height)});
                        const std::int64_t image_limit = static_cast<std::int64_t>(std::min<std::size_t>(
                            options_.max_image_pixels, static_cast<std::size_t>(std::numeric_limits<int>::max())));
                        if (pixels > image_limit) {
                            diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child Sixel image exceeds configured pixel limit");
                        } else {
                            VirtualDisplay display(cells_, profile_.cell_pixels);
                            if (display.write("\x1bP" + control_ + "\x1b\\") && display.has_raster_pixels()) {
                                const std::optional<CroppedRaster> cropped =
                                    crop_visible_raster(display.raster_plane(), profile_.cell_pixels);
                                if (!cropped) {
                                    diagnostic(TerminalDiagnostic::Kind::MalformedSequence,
                                               "child Sixel sequence contained no visible pixels");
                                    control_.clear();
                                    parse_state_ = ParseState::Ground;
                                    break;
                                }
                                rasters_.clear();
                                rasters_.push_back(TerminalRaster{
                                    raster_identity_,
                                    Point{cursor_.position.x + cropped->cell_offset.x,
                                          cursor_.position.y + cropped->cell_offset.y},
                                    cropped->cell_extent, cropped->image, "[sixel]"});
                            } else {
                                diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "malformed child Sixel sequence");
                            }
                        }
                    } else {
                        diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "child DCS sequence ignored");
                    }
                    control_.clear();
                    dcs_sixel_payload_ = false;
                    parse_state_ = ParseState::Ground;
                }
                else {
                    const bool retained_escape = append_dcs_byte('\x1b');
                    const bool retained_byte = retained_escape && append_dcs_byte(byte);
                    if (retained_byte) parse_state_ = ParseState::Dcs;
                }
                break;
            case ParseState::Discard:
                if (byte == '\x1b') parse_state_ = ParseState::DiscardEscape;
                break;
            case ParseState::DiscardEscape:
                parse_state_ = byte == '\\' ? ParseState::Ground : ParseState::Discard;
                break;
        }
    }
    pending_output_.erase(0, budget);
    put_text();
}

int TerminalEmulator::parameter(std::string_view text, std::size_t index, int default_value) {
    if (!text.empty() && text.front() == '?') text.remove_prefix(1);
    std::size_t first = 0;
    for (std::size_t current = 0; current < index; ++current) {
        const std::size_t next = text.find(';', first);
        if (next == std::string_view::npos) return default_value;
        first = next + 1;
    }
    const std::size_t last = text.find(';', first);
    const std::string_view value = text.substr(first, last == std::string_view::npos ? text.size() - first : last - first);
    if (value.empty()) return default_value;
    int parsed = default_value;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() ? parsed : default_value;
}

void TerminalEmulator::handle_csi(char final_byte) {
    // DECSCUSR (CSI Ps SP q) is emitted by interactive shells to select a
    // steady/blinking cursor style. The core records the declaration; the
    // outer presenter emits the host terminal's corresponding mode.
    if (final_byte == 'q' && !control_.empty() && control_.back() == ' ') {
        const int cursor_style = parameter(std::string_view(control_).substr(0, control_.size() - 1), 0, 0);
        cursor_.blink = (cursor_style == 1 || cursor_style == 3 || cursor_style == 5);
        if (cursor_style == 3 || cursor_style == 4)
            cursor_.shape = CursorShape::Underline;
        else if (cursor_style == 5 || cursor_style == 6)
            cursor_.shape = CursorShape::Bar;
        else
            cursor_.shape = CursorShape::Block;
        return;
    }
    // Capability probes emitted by a nested ckVision child are private to
    // that child endpoint.  The profile deliberately does not expose the
    // outer terminal's probe state, so these queries receive no reply but
    // are still consumed as well-formed input rather than diagnostics.
    if (final_byte == 'p' && control_.ends_with('$') &&
        (control_ == "?2026$" || control_ == "?2031$" || control_ == "?1016$"))
        return;
    if (final_byte == 'c' && control_.empty()) {
        if (profile_.query_policy == TerminalQueryPolicy::DeclaredProfile)
            send_input("\x1b[?1;2c");
        return;
    }
    if (final_byte == 't' && control_ == "16") {
        if (profile_.query_policy == TerminalQueryPolicy::DeclaredProfile)
            send_input("\x1b[6;" + std::to_string(std::max(1, profile_.cell_pixels.height)) + ";" +
                       std::to_string(std::max(1, profile_.cell_pixels.width)) + "t");
        return;
    }
    if (final_byte == 't' && control_ == "14") {
        // XTWINOPS 14: the text area's total pixel size — cell metric times
        // the grid, exactly as a real terminal derives it.
        if (profile_.query_policy == TerminalQueryPolicy::DeclaredProfile)
            send_input("\x1b[4;" + std::to_string(std::max(1, profile_.cell_pixels.height) * cells_.height) +
                       ";" + std::to_string(std::max(1, profile_.cell_pixels.width) * cells_.width) + "t");
        return;
    }
    if (final_byte == 's') {
        saved_cursor_ = cursor_;
        saved_cursor_valid_ = true;
        return;
    }
    if (final_byte == 'u') {
        if (saved_cursor_valid_) cursor_ = saved_cursor_;
        return;
    }
    if ((final_byte == 'h' || final_byte == 'l') && !control_.empty() && control_.front() == '?') {
        const bool enabled = final_byte == 'h';
        std::string_view modes(control_);
        modes.remove_prefix(1);
        std::size_t begin = 0;
        while (begin <= modes.size()) {
            const std::size_t end = modes.find(';', begin);
            const std::string_view value = modes.substr(begin, end == std::string_view::npos ? modes.size() - begin : end - begin);
            const int mode = parameter(value, 0, -1);
            if (mode == 1049) {
                if (enabled) {
                    saved_cursor_ = cursor_;
                    saved_cursor_valid_ = true;
                    alternate_buffer_ = true;
                    reset_active_buffer();
                    rasters_.clear();
                    cursor_.position = {};
                } else {
                    alternate_buffer_ = false;
                    rasters_.clear();
                    if (saved_cursor_valid_) cursor_ = saved_cursor_;
                }
            } else if (mode == 47 || mode == 1047) {
                alternate_buffer_ = enabled;
                if (enabled) {
                    reset_active_buffer();
                    rasters_.clear();
                    cursor_.position = {};
                } else {
                    rasters_.clear();
                }
            } else if (mode == 1048) {
                if (enabled) {
                    saved_cursor_ = cursor_;
                    saved_cursor_valid_ = true;
                } else if (saved_cursor_valid_) {
                    cursor_ = saved_cursor_;
                }
            } else if (mode == 2004) {
                bracketed_paste_enabled_ = enabled && profile_.bracketed_paste;
            } else if (mode == 6) {
                origin_mode_ = enabled;
                cursor_.position = Point{0, origin_mode_ ? scroll_top_ : 0};
            } else if (mode == 1) {
                application_cursor_keys_ = enabled;
            } else if (mode == 1000 || mode == 1002 || mode == 1003) {
                mouse_reporting_enabled_ = enabled && profile_.mouse_reporting;
                if (!mouse_reporting_enabled_) mouse_sgr_enabled_ = false;
            } else if (mode == 1006) {
                mouse_sgr_enabled_ = enabled && profile_.mouse_reporting;
            } else if (mode == 1004) {
                focus_reporting_enabled_ = enabled;
            } else if (mode == 25) {
                cursor_.visible = enabled;
            }
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }
        return;
    }
    if (final_byte == 'H' || final_byte == 'f') {
        const int origin_top = origin_mode_ ? scroll_top_ : 0;
        const int origin_bottom = origin_mode_ ? scroll_bottom_ : cells_.height;
        cursor_.position.y = std::clamp(origin_top + parameter(control_, 0, 1) - 1, origin_top, origin_bottom - 1);
        cursor_.position.x = std::clamp(parameter(control_, 1, 1) - 1, 0, cells_.width - 1);
        return;
    }
    if (final_byte == 'G' || final_byte == '`') {
        cursor_.position.x = std::clamp(parameter(control_, 0, 1) - 1, 0, cells_.width - 1);
        return;
    }
    if (final_byte == 'd') {
        const int origin_top = origin_mode_ ? scroll_top_ : 0;
        const int origin_bottom = origin_mode_ ? scroll_bottom_ : cells_.height;
        cursor_.position.y = std::clamp(origin_top + parameter(control_, 0, 1) - 1,
                                         origin_top, origin_bottom - 1);
        return;
    }
    if (final_byte == 'E') {
        cursor_.position.y = std::min((origin_mode_ ? scroll_bottom_ : cells_.height) - 1,
                                      cursor_.position.y + parameter(control_, 0, 1));
        cursor_.position.x = 0;
        return;
    }
    if (final_byte == 'F') {
        cursor_.position.y = std::max(origin_mode_ ? scroll_top_ : 0,
                                      cursor_.position.y - parameter(control_, 0, 1));
        cursor_.position.x = 0;
        return;
    }
    if (final_byte == 'A') { cursor_.position.y = std::max(0, cursor_.position.y - parameter(control_, 0, 1)); return; }
    if (final_byte == 'B') { cursor_.position.y = std::min(cells_.height - 1, cursor_.position.y + parameter(control_, 0, 1)); return; }
    if (final_byte == 'C') { cursor_.position.x = std::min(cells_.width - 1, cursor_.position.x + parameter(control_, 0, 1)); return; }
    if (final_byte == 'D') { cursor_.position.x = std::max(0, cursor_.position.x - parameter(control_, 0, 1)); return; }
    if (final_byte == '@') { insert_cells(parameter(control_, 0, 1)); return; }
    if (final_byte == 'P') { delete_cells(parameter(control_, 0, 1)); return; }
    if (final_byte == 'L') {
        scroll_down_region(cursor_.position.y, scroll_bottom_, parameter(control_, 0, 1));
        return;
    }
    if (final_byte == 'M') {
        scroll_up_region(cursor_.position.y, scroll_bottom_, parameter(control_, 0, 1));
        return;
    }
    if (final_byte == 'X') {
        const int count = std::clamp(parameter(control_, 0, 1), 0, cells_.width - cursor_.position.x);
        erase_cells(cursor_.position.x, cursor_.position.y, cursor_.position.x + count, cursor_.position.y + 1);
        return;
    }
    if (final_byte == 'J') {
        const int mode = parameter(control_, 0, 0);
        if (mode == 0) erase_cells(cursor_.position.x, cursor_.position.y, cells_.width, cursor_.position.y + 1);
        if (mode == 0 && cursor_.position.y + 1 < cells_.height)
            erase_cells(0, cursor_.position.y + 1, cells_.width, cells_.height);
        else if (mode == 1) {
            if (cursor_.position.y > 0) erase_cells(0, 0, cells_.width, cursor_.position.y);
            erase_cells(0, cursor_.position.y, cursor_.position.x + 1, cursor_.position.y + 1);
        } else if (mode == 2) {
            erase_cells(0, 0, cells_.width, cells_.height);
            rasters_.clear();
        } else if (mode == 3) {
            scrollback_.clear();
        }
        return;
    }
    if (final_byte == 'K') {
        const int mode = parameter(control_, 0, 0);
        if (mode == 0) erase_cells(cursor_.position.x, cursor_.position.y, cells_.width, cursor_.position.y + 1);
        else if (mode == 1) erase_cells(0, cursor_.position.y, cursor_.position.x + 1, cursor_.position.y + 1);
        else if (mode == 2) erase_cells(0, cursor_.position.y, cells_.width, cursor_.position.y + 1);
        return;
    }
    if (final_byte == 'r') {
        const int top = std::clamp(parameter(control_, 0, 1) - 1, 0, cells_.height - 1);
        const int bottom = std::clamp(parameter(control_, 1, cells_.height), 1, cells_.height);
        if (top >= bottom) return;
        scroll_top_ = top;
        scroll_bottom_ = bottom;
        cursor_.position = Point{0, origin_mode_ ? scroll_top_ : 0};
        return;
    }
    if (final_byte == 'S') { scroll_up_region(scroll_top_, scroll_bottom_, parameter(control_, 0, 1)); return; }
    if (final_byte == 'T') { scroll_down_region(scroll_top_, scroll_bottom_, parameter(control_, 0, 1)); return; }
    if (final_byte == 'm') {
        std::vector<int> values;
        std::size_t begin = 0;
        bool valid = true;
        while (begin <= control_.size()) {
            const std::size_t end = control_.find(';', begin);
            const std::string_view text = std::string_view(control_).substr(
                begin, end == std::string::npos ? control_.size() - begin : end - begin);
            int value = 0;
            if (!text.empty()) {
                const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
                if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
                    valid = false;
                    break;
                }
            }
            values.push_back(value);
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        if (!valid || values.empty()) {
            diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "malformed child SGR sequence");
            return;
        }
        const auto clear_attrs = [this](Attr attrs) {
            style_.attrs = static_cast<Attr>(static_cast<std::uint8_t>(style_.attrs) &
                                             static_cast<std::uint8_t>(~static_cast<std::uint8_t>(attrs)));
        };
        for (std::size_t index = 0; index < values.size(); ++index) {
            const int value = values[index];
            if (value == 0) style_ = profile_.default_style;
            else if (value == 1) style_.attrs |= Attr::Bold;
            else if (value == 2) style_.attrs |= Attr::Dim;
            else if (value == 3) style_.attrs |= Attr::Italic;
            else if (value == 4) style_.attrs |= Attr::Underline;
            else if (value == 7) style_.attrs |= Attr::Reverse;
            else if (value == 9) style_.attrs |= Attr::Strike;
            else if (value == 22) clear_attrs(Attr::Bold | Attr::Dim);
            else if (value == 23) clear_attrs(Attr::Italic);
            else if (value == 24) clear_attrs(Attr::Underline);
            else if (value == 27) clear_attrs(Attr::Reverse);
            else if (value == 29) clear_attrs(Attr::Strike);
            else if (value == 39) style_.fg = profile_.default_style.fg;
            else if (value == 49) style_.bg = profile_.default_style.bg;
            else if ((value >= 30 && value <= 37) || (value >= 90 && value <= 97))
                style_.fg = ansi_color(value >= 90 ? value - 90 + 8 : value - 30);
            else if ((value >= 40 && value <= 47) || (value >= 100 && value <= 107))
                style_.bg = ansi_color(value >= 100 ? value - 100 + 8 : value - 40);
            else if (value == 38 || value == 48) {
                const bool background = value == 48;
                if (index + 1 >= values.size()) {
                    diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "truncated child extended color");
                    return;
                }
                const int mode = values[++index];
                Color color;
                if (mode == 5 && index + 1 < values.size() && values[index + 1] >= 0 && values[index + 1] <= 255) {
                    color = ansi_color(values[++index]);
                } else if (mode == 2 && index + 3 < values.size() && values[index + 1] >= 0 &&
                           values[index + 1] <= 255 && values[index + 2] >= 0 && values[index + 2] <= 255 &&
                           values[index + 3] >= 0 && values[index + 3] <= 255) {
                    color = Color::rgb(static_cast<std::uint8_t>(values[index + 1]),
                                       static_cast<std::uint8_t>(values[index + 2]),
                                       static_cast<std::uint8_t>(values[index + 3]));
                    index += 3;
                } else {
                    diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "invalid child extended color");
                    return;
                }
                if (background) style_.bg = color;
                else style_.fg = color;
            } else if (value != 5 && value != 6) {
                diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child SGR parameter");
            }
        }
        return;
    }
    if (final_byte == 'n' && profile_.query_policy == TerminalQueryPolicy::DeclaredProfile) {
        const int query = parameter(control_, 0, 0);
        if (query == 5) send_input("\x1b[0n");
        else if (query == 6)
            send_input("\x1b[" + std::to_string(cursor_.position.y + 1) + ";" +
                       std::to_string(cursor_.position.x + 1) + "R");
        return;
    }
    diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child CSI sequence");
}

void TerminalEmulator::put_text(bool flush_incomplete) {
    const std::size_t ready = flush_incomplete ? printable_.size() : complete_utf8_prefix(printable_);
    const std::string_view ready_text(printable_.data(), ready);
    std::size_t position = 0;
    while (position < ready) {
        const std::size_t end = text::grapheme_end(ready_text, position);
        if (end <= position) break;
        put_grapheme(std::string_view(printable_).substr(position, end - position));
        position = end;
    }
    printable_.erase(0, position);
}

void TerminalEmulator::put_grapheme(std::string_view grapheme) {
    if (cursor_.position.x >= cells_.width) { cursor_.position.x = 0; index(); }
    Cell cell = Cell::from_grapheme(grapheme, style_);
    active_cells()[static_cast<std::size_t>(cursor_.position.y * cells_.width + cursor_.position.x)] = cell;
    const int width = std::max(1, cell.width());
    if (width == 2 && cursor_.position.x + 1 < cells_.width)
        active_cells()[static_cast<std::size_t>(cursor_.position.y * cells_.width + cursor_.position.x + 1)] = Cell::continuation(style_);
    cursor_.position.x += width;
}

void TerminalEmulator::erase_cells(int left, int top, int right, int bottom) noexcept {
    left = std::clamp(left, 0, cells_.width); right = std::clamp(right, 0, cells_.width);
    top = std::clamp(top, 0, cells_.height); bottom = std::clamp(bottom, 0, cells_.height);
    auto& cells = active_cells();
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            cells[static_cast<std::size_t>(y * cells_.width + x)] = Cell::from_grapheme(" ", profile_.default_style);
}

void TerminalEmulator::insert_cells(int count) noexcept {
    count = std::clamp(count, 0, cells_.width - cursor_.position.x);
    if (count == 0) return;
    std::vector<Cell>& cells = active_cells();
    auto first = cells.begin() + static_cast<std::ptrdiff_t>(cursor_.position.y * cells_.width + cursor_.position.x);
    auto last = cells.begin() + static_cast<std::ptrdiff_t>((cursor_.position.y + 1) * cells_.width);
    std::move_backward(first, last - count, last);
    std::fill(first, first + count, Cell::from_grapheme(" ", profile_.default_style));
}

void TerminalEmulator::delete_cells(int count) noexcept {
    count = std::clamp(count, 0, cells_.width - cursor_.position.x);
    if (count == 0) return;
    std::vector<Cell>& cells = active_cells();
    auto first = cells.begin() + static_cast<std::ptrdiff_t>(cursor_.position.y * cells_.width + cursor_.position.x);
    auto last = cells.begin() + static_cast<std::ptrdiff_t>((cursor_.position.y + 1) * cells_.width);
    std::move(first + count, last, first);
    std::fill(last - count, last, Cell::from_grapheme(" ", profile_.default_style));
}

void TerminalEmulator::scroll_up(int rows) noexcept {
    scroll_up_region(0, cells_.height, rows);
}

void TerminalEmulator::scroll_up_region(int top, int bottom, int rows) noexcept {
    top = std::clamp(top, 0, cells_.height);
    bottom = std::clamp(bottom, top, cells_.height);
    rows = std::clamp(rows, 0, bottom - top);
    if (rows == 0) return;
    auto& cells = active_cells();
    if (!alternate_buffer_ && top == 0 && bottom == cells_.height) {
        for (int row = 0; row < rows; ++row) {
            scrollback_.insert(scrollback_.end(), cells.begin() + static_cast<std::ptrdiff_t>(row * cells_.width),
                               cells.begin() + static_cast<std::ptrdiff_t>((row + 1) * cells_.width));
        }
        const std::size_t maximum = options_.max_scrollback_lines * static_cast<std::size_t>(cells_.width);
        if (scrollback_.size() > maximum) scrollback_.erase(scrollback_.begin(), scrollback_.begin() + static_cast<std::ptrdiff_t>(scrollback_.size() - maximum));
    }
    const auto first = cells.begin() + static_cast<std::ptrdiff_t>(top * cells_.width);
    const auto last = cells.begin() + static_cast<std::ptrdiff_t>(bottom * cells_.width);
    std::move(first + static_cast<std::ptrdiff_t>(rows * cells_.width), last, first);
    std::fill(last - static_cast<std::ptrdiff_t>(rows * cells_.width), last,
              Cell::from_grapheme(" ", profile_.default_style));
}

void TerminalEmulator::scroll_down_region(int top, int bottom, int rows) noexcept {
    top = std::clamp(top, 0, cells_.height);
    bottom = std::clamp(bottom, top, cells_.height);
    rows = std::clamp(rows, 0, bottom - top);
    if (rows == 0) return;
    auto& cells = active_cells();
    const auto first = cells.begin() + static_cast<std::ptrdiff_t>(top * cells_.width);
    const auto last = cells.begin() + static_cast<std::ptrdiff_t>(bottom * cells_.width);
    std::move_backward(first, last - static_cast<std::ptrdiff_t>(rows * cells_.width), last);
    std::fill(first, first + static_cast<std::ptrdiff_t>(rows * cells_.width),
              Cell::from_grapheme(" ", profile_.default_style));
}

void TerminalEmulator::index() noexcept {
    if (cursor_.position.y == scroll_bottom_ - 1) {
        scroll_up_region(scroll_top_, scroll_bottom_, 1);
    } else {
        cursor_.position.y = std::min(cells_.height - 1, cursor_.position.y + 1);
    }
}

void TerminalEmulator::reverse_index() noexcept {
    if (cursor_.position.y == scroll_top_) {
        scroll_down_region(scroll_top_, scroll_bottom_, 1);
    } else {
        cursor_.position.y = std::max(0, cursor_.position.y - 1);
    }
}

void TerminalEmulator::reset_active_buffer() {
    std::fill(active_cells().begin(), active_cells().end(), Cell::from_grapheme(" ", profile_.default_style));
}

void TerminalEmulator::resize(Size cells, Size cell_pixels) {
    const Size old_cells = cells_;
    const Size new_cells = bounded_size(cells, options_.max_cells);
    const Style default_style = profile_.default_style;
    auto resized_buffer = [old_cells, new_cells, default_style](const std::vector<Cell>& source) {
        std::vector<Cell> result(static_cast<std::size_t>(new_cells.width * new_cells.height),
                                 Cell::from_grapheme(" ", default_style));
        const int rows = std::min(old_cells.height, new_cells.height);
        const int columns = std::min(old_cells.width, new_cells.width);
        for (int y = 0; y < rows; ++y)
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(y * old_cells.width), columns,
                        result.begin() + static_cast<std::ptrdiff_t>(y * new_cells.width));
        return result;
    };

    primary_ = resized_buffer(primary_);
    alternate_ = resized_buffer(alternate_);
    cells_ = new_cells;
    profile_.cells = cells_;
    profile_.cell_pixels = {std::max(1, cell_pixels.width), std::max(1, cell_pixels.height)};
    // A resize changes the cell-to-pixel mapping, but it does not erase the
    // child's private image state. Keep each decoded image and rebuild only
    // its cell placement; a later child erase/alternate-buffer transition
    // still removes it explicitly.
    for (TerminalRaster& raster : rasters_) {
        if (raster.image == nullptr) continue;
        raster.cell_extent = Size{
            std::max(1, ceil_div_positive(raster.image->width(), profile_.cell_pixels.width)),
            std::max(1, ceil_div_positive(raster.image->height(), profile_.cell_pixels.height))};
    }
    scroll_top_ = 0;
    scroll_bottom_ = cells_.height;
    origin_mode_ = false;
    cursor_.position.x = std::clamp(cursor_.position.x, 0, cells_.width - 1);
    cursor_.position.y = std::clamp(cursor_.position.y, 0, cells_.height - 1);
}

void TerminalEmulator::send_input(std::string_view bytes) {
    if (state_ == TerminalSubsessionState::Closed || state_ == TerminalSubsessionState::Failed ||
        state_ == TerminalSubsessionState::Exited)
        return;
    const std::size_t remaining = options_.max_input_bytes > pending_input_.size() ? options_.max_input_bytes - pending_input_.size() : 0;
    pending_input_.append(bytes.substr(0, remaining));
    if (bytes.size() > remaining) diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child input queue exceeded configured limit");
}

std::string TerminalEmulator::take_pending_input() { std::string result = std::move(pending_input_); pending_input_.clear(); return result; }
void TerminalEmulator::mark_exited(int exit_code) {
    while (!pending_output_.empty()) {
        const std::size_t before = pending_output_.size();
        feed_output({});
        if (pending_output_.size() == before) break;
    }
    put_text(true);
    if (parse_state_ != ParseState::Ground)
        diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "child exited with incomplete terminal control sequence");
    parse_state_ = ParseState::Ground;
    control_.clear();
    state_ = TerminalSubsessionState::Exited;
    exit_code_ = exit_code;
}

void TerminalEmulator::mark_failed(std::string message) {
    state_ = TerminalSubsessionState::Failed;
    diagnostic(TerminalDiagnostic::Kind::ChildExited, std::move(message));
}

}  // namespace ckv::term
