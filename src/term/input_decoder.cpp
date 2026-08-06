// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Coverage and every scope decision: docs/input-decoder.md.
#include "cvision/term/input_decoder.hpp"

#include <algorithm>
#include <climits>
#include <optional>

#include "cvision/core/utf8.hpp"

namespace ckv::term {
namespace {

int expected_utf8_length(unsigned char b0) noexcept {
    if (b0 < 0x80) return 1;
    if ((b0 & 0xE0) == 0xC0) return 2;
    if ((b0 & 0xF0) == 0xE0) return 3;
    if ((b0 & 0xF8) == 0xF0) return 4;
    return 0;
}

Modifier decode_modifier_param(int m) noexcept {
    if (m <= 1) return Modifier::None;
    const int bits = m - 1;
    Modifier mod = Modifier::None;
    if (bits & 1) mod = mod | Modifier::Shift;
    if (bits & 2) mod = mod | Modifier::Alt;
    if (bits & 4) mod = mod | Modifier::Ctrl;
    if (bits & 8) mod = mod | Modifier::Super;
    return mod;
}

std::vector<int> parse_params(std::string_view s) {
    std::vector<int> params;
    if (s.empty()) return params;
    std::size_t start = 0;
    while (start <= s.size()) {
        const std::size_t semi = s.find(';', start);
        const std::string_view field =
            s.substr(start, semi == std::string_view::npos ? std::string_view::npos : semi - start);
        // Saturating accumulation: a hostile or malformed sequence can
        // supply an arbitrarily long digit run (CSI/OSC parameters are
        // conventionally a few digits at most), and naive
        // value*10+digit overflow on signed int is undefined behavior,
        // not just wraparound — confirmed reachable and UBSan-flagged
        // for a real overlong parameter before this fix.
        int value = 0;
        for (const char c : field) {
            if (c < '0' || c > '9') continue;
            if (value > (INT_MAX - (c - '0')) / 10) {
                value = INT_MAX;
                continue;
            }
            value = value * 10 + (c - '0');
        }
        params.push_back(value);
        if (semi == std::string_view::npos) break;
        start = semi + 1;
    }
    return params;
}

KeyChord control_key_chord(unsigned char b) {
    switch (b) {
        case 0x09: return KeyChord{Key::Tab, Modifier::None, {}};
        case 0x0D: return KeyChord{Key::Enter, Modifier::None, {}};
        case 0x7F:
        case 0x08: return KeyChord{Key::Backspace, Modifier::None, {}};
        default: break;
    }
    if (b >= 0x01 && b <= 0x1A) {
        const char letter = static_cast<char>('a' + (b - 1));
        return KeyChord{Key::Char, Modifier::Ctrl, std::string(1, letter)};
    }
    return KeyChord{Key::None, Modifier::None, {}};
}

KeyChord key_from_codepoint(int codepoint, Modifier mod) {
    switch (codepoint) {
        case 13: return KeyChord{Key::Enter, mod, {}};
        case 9: return KeyChord{Key::Tab, mod, {}};
        case 27: return KeyChord{Key::Escape, mod, {}};
        case 127:
        case 8: return KeyChord{Key::Backspace, mod, {}};
        default: break;
    }
    if (codepoint < 0 || codepoint > 0x10FFFF) codepoint = static_cast<int>(utf8::replacement_char);
    std::string text;
    utf8::encode(static_cast<char32_t>(codepoint), text);
    return KeyChord{Key::Char, mod, text};
}

std::optional<Key> tilde_key_from_number(int n) noexcept {
    switch (n) {
        case 1: return Key::Home;
        case 2: return Key::Insert;
        case 3: return Key::Delete;
        case 4: return Key::End;
        case 5: return Key::PageUp;
        case 6: return Key::PageDown;
        case 11: return Key::F1;
        case 12: return Key::F2;
        case 13: return Key::F3;
        case 14: return Key::F4;
        case 15: return Key::F5;
        case 17: return Key::F6;
        case 18: return Key::F7;
        case 19: return Key::F8;
        case 20: return Key::F9;
        case 21: return Key::F10;
        case 23: return Key::F11;
        case 24: return Key::F12;
        default: return std::nullopt;
    }
}

// D-023's paste sanitization rule — distinct from
// ckv::text::sanitize_display_text: tab and newline are exempt here
// because paste inserts multi-line text, unlike a single-grapheme Cell.
std::string sanitize_paste(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = pos;
        const char32_t cp = utf8::decode(text, pos);
        const bool is_tab_or_nl = (cp == 0x09 || cp == 0x0A);
        const bool is_c0 = cp <= 0x1F;
        const bool is_c1 = cp >= 0x80 && cp <= 0x9F;
        if ((is_c0 && !is_tab_or_nl) || is_c1 || cp == utf8::replacement_char) {
            out += "\xEF\xBF\xBD";
        } else {
            out.append(text.substr(start, pos - start));
        }
    }
    return out;
}

// "rgb:RRRR/GGGG/BBBB" (1-4 hex digits per channel) -> relative
// luminance in [0,1], or nullopt if the spec doesn't parse.
std::optional<double> parse_rgb_luminance(std::string_view pt) {
    if (pt.size() < 6 || pt.substr(0, 4) != "rgb:") return std::nullopt;
    const std::string_view rest = pt.substr(4);
    const std::size_t p1 = rest.find('/');
    if (p1 == std::string_view::npos) return std::nullopt;
    const std::size_t p2 = rest.find('/', p1 + 1);
    if (p2 == std::string_view::npos) return std::nullopt;
    const std::string_view r_hex = rest.substr(0, p1);
    const std::string_view g_hex = rest.substr(p1 + 1, p2 - p1 - 1);
    const std::string_view b_hex = rest.substr(p2 + 1);

    const auto hex_to_frac = [](std::string_view h) -> double {
        if (h.empty() || h.size() > 4) return -1.0;
        int value = 0;
        for (const char c : h) {
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return -1.0;
            value = value * 16 + d;
        }
        const double max_value = static_cast<double>((1u << (4 * h.size())) - 1);
        return static_cast<double>(value) / max_value;
    };
    const double r = hex_to_frac(r_hex);
    const double g = hex_to_frac(g_hex);
    const double b = hex_to_frac(b_hex);
    if (r < 0 || g < 0 || b < 0) return std::nullopt;
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

}  // namespace

std::vector<TerminalEvent> InputDecoder::feed(std::string_view data, std::int64_t now_nanos) {
    pending_.append(data);
    return drain(now_nanos);
}

std::vector<TerminalEvent> InputDecoder::poll_timeout(std::int64_t now_nanos) {
    return drain(now_nanos);
}

std::vector<TerminalEvent> InputDecoder::drain(std::int64_t now_nanos) {
    std::vector<TerminalEvent> events;
    while (!pending_.empty()) {
        const ParseResult r = try_parse_one(pending_, now_nanos);
        if (r.status == Status::Incomplete) break;
        if (r.status == Status::Invalid) {
            pending_.erase(0, 1);
            continue;
        }
        if (r.updated_caps) {
            caps_ = *r.updated_caps;
            events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
        }
        if (r.event) events.push_back(std::move(*r.event));
        pending_.erase(0, r.consumed);
    }
    return events;
}

InputDecoder::ParseResult InputDecoder::try_parse_one(std::string_view buf, std::int64_t now_nanos) {
    if (in_paste_) return parse_paste_continuation(buf);
    const unsigned char b0 = static_cast<unsigned char>(buf[0]);
    if (b0 == 0x1B) return parse_escape(buf, now_nanos);
    return parse_utf8_or_control(buf);
}

InputDecoder::ParseResult InputDecoder::parse_utf8_or_control(std::string_view buf) {
    const unsigned char b0 = static_cast<unsigned char>(buf[0]);
    if (b0 < 0x20) {
        const KeyChord chord = control_key_chord(b0);
        if (chord.key == Key::None) return {Status::Complete, 1, std::nullopt, std::nullopt};
        return {Status::Complete, 1, TerminalEvent{KeyEvent{chord}}, std::nullopt};
    }
    if (b0 == 0x7F)
        return {Status::Complete, 1,
                TerminalEvent{KeyEvent{KeyChord{Key::Backspace, Modifier::None, {}}}}, std::nullopt};

    const int expected = expected_utf8_length(b0);
    if (expected == 0) return {Status::Invalid, 0, std::nullopt, std::nullopt};

    // Validate every continuation byte actually available, even short
    // of `expected` bytes: an invalid continuation byte must resync
    // immediately. Waiting for the full expected length before
    // checking would block forever ("Incomplete") on a lead byte
    // followed by bytes that can never complete it — a real hang risk
    // if no further input ever arrives.
    const std::size_t available = std::min(buf.size(), static_cast<std::size_t>(expected));
    for (std::size_t i = 1; i < available; ++i) {
        const unsigned char b = static_cast<unsigned char>(buf[i]);
        if ((b & 0xC0) != 0x80) return {Status::Invalid, 0, std::nullopt, std::nullopt};
    }
    if (buf.size() < static_cast<std::size_t>(expected))
        return {Status::Incomplete, 0, std::nullopt, std::nullopt};

    std::size_t pos = 0;
    const char32_t cp = utf8::decode(buf, pos);
    std::string text;
    utf8::encode(cp, text);
    return {Status::Complete, pos, TerminalEvent{KeyEvent{KeyChord{Key::Char, Modifier::None, text}}},
            std::nullopt};
}

InputDecoder::ParseResult InputDecoder::parse_escape(std::string_view buf, std::int64_t now_nanos) {
    if (buf.size() == 1) {
        if (esc_first_seen_nanos_ < 0) esc_first_seen_nanos_ = now_nanos;
        if (now_nanos - esc_first_seen_nanos_ >= kEscTimeoutNanos) {
            esc_first_seen_nanos_ = -1;
            return {Status::Complete, 1,
                    TerminalEvent{KeyEvent{KeyChord{Key::Escape, Modifier::None, {}}}}, std::nullopt};
        }
        return {Status::Incomplete, 0, std::nullopt, std::nullopt};
    }
    esc_first_seen_nanos_ = -1;

    const char b1 = buf[1];
    if (b1 == '[') return parse_csi(buf);
    if (b1 == 'O') return parse_ss3(buf);
    if (b1 == ']') return parse_osc(buf);
    if (b1 == 0x1B)
        return {Status::Complete, 1, TerminalEvent{KeyEvent{KeyChord{Key::Escape, Modifier::None, {}}}},
                std::nullopt};

    // Legacy Alt+key: ESC followed by one otherwise-ordinary byte.
    const ParseResult inner = parse_utf8_or_control(buf.substr(1));
    if (inner.status == Status::Incomplete) return inner;
    if (inner.status == Status::Invalid)
        return {Status::Complete, 1, TerminalEvent{KeyEvent{KeyChord{Key::Escape, Modifier::None, {}}}},
                std::nullopt};
    if (inner.event) {
        if (const auto* key_event = std::get_if<KeyEvent>(&*inner.event)) {
            KeyChord chord = key_event->chord;
            chord.modifiers = chord.modifiers | Modifier::Alt;
            return {Status::Complete, 1 + inner.consumed, TerminalEvent{KeyEvent{chord}}, std::nullopt};
        }
    }
    return {Status::Complete, 1 + inner.consumed, std::nullopt, std::nullopt};
}

InputDecoder::ParseResult InputDecoder::parse_ss3(std::string_view buf) {
    if (buf.size() < 3) return {Status::Incomplete, 0, std::nullopt, std::nullopt};
    std::optional<Key> key;
    switch (buf[2]) {
        case 'P': key = Key::F1; break;
        case 'Q': key = Key::F2; break;
        case 'R': key = Key::F3; break;
        case 'S': key = Key::F4; break;
        case 'A': key = Key::Up; break;
        case 'B': key = Key::Down; break;
        case 'C': key = Key::Right; break;
        case 'D': key = Key::Left; break;
        case 'H': key = Key::Home; break;
        case 'F': key = Key::End; break;
        default: break;
    }
    if (!key) return {Status::Complete, 3, std::nullopt, std::nullopt};
    return {Status::Complete, 3, TerminalEvent{KeyEvent{KeyChord{*key, Modifier::None, {}}}},
            std::nullopt};
}

InputDecoder::ParseResult InputDecoder::parse_csi(std::string_view buf) {
    std::size_t pos = 2;
    bool private_marker = false;
    char marker = 0;
    if (pos < buf.size() &&
        (buf[pos] == '?' || buf[pos] == '<' || buf[pos] == '>' || buf[pos] == '=')) {
        marker = buf[pos];
        private_marker = true;
        ++pos;
    }
    std::string param_str;
    while (pos < buf.size() && ((buf[pos] >= '0' && buf[pos] <= '9') || buf[pos] == ';' ||
                                 buf[pos] == ':')) {
        param_str += (buf[pos] == ':' ? ';' : buf[pos]);
        ++pos;
    }
    while (pos < buf.size() && static_cast<unsigned char>(buf[pos]) >= 0x20 &&
           static_cast<unsigned char>(buf[pos]) <= 0x2F)
        ++pos;
    if (pos >= buf.size()) return {Status::Incomplete, 0, std::nullopt, std::nullopt};
    const unsigned char final_byte = static_cast<unsigned char>(buf[pos]);
    if (final_byte < 0x40 || final_byte > 0x7E)
        return {Status::Invalid, 0, std::nullopt, std::nullopt};
    ++pos;
    const std::size_t consumed = pos;
    const std::vector<int> params = parse_params(param_str);

    if (final_byte == '~') {
        if (params.size() >= 1 && params[0] == 200) {
            in_paste_ = true;
            paste_accum_.clear();
            return {Status::Complete, consumed, std::nullopt, std::nullopt};
        }
        if (params.size() >= 1 && params[0] == 201)
            return {Status::Complete, consumed, std::nullopt, std::nullopt};  // stray end marker
        if (params.size() == 3 && params[0] == 27) {
            const Modifier mod = decode_modifier_param(params[1]);
            return {Status::Complete, consumed, TerminalEvent{KeyEvent{key_from_codepoint(params[2], mod)}},
                    std::nullopt};
        }
        if (!params.empty()) {
            const Modifier mod = params.size() >= 2 ? decode_modifier_param(params[1]) : Modifier::None;
            if (const auto key = tilde_key_from_number(params[0]))
                return {Status::Complete, consumed,
                        TerminalEvent{KeyEvent{KeyChord{*key, mod, {}}}}, std::nullopt};
        }
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    if (final_byte == 'u') {
        if (!params.empty()) {
            const Modifier mod = params.size() >= 2 ? decode_modifier_param(params[1]) : Modifier::None;
            return {Status::Complete, consumed,
                    TerminalEvent{KeyEvent{key_from_codepoint(params[0], mod)}}, std::nullopt};
        }
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    if (!private_marker) {
        const Modifier mod = params.size() >= 2 ? decode_modifier_param(params[1]) : Modifier::None;
        std::optional<Key> key;
        switch (final_byte) {
            case 'A': key = Key::Up; break;
            case 'B': key = Key::Down; break;
            case 'C': key = Key::Right; break;
            case 'D': key = Key::Left; break;
            case 'H': key = Key::Home; break;
            case 'F': key = Key::End; break;
            default: break;
        }
        if (key)
            return {Status::Complete, consumed, TerminalEvent{KeyEvent{KeyChord{*key, mod, {}}}},
                    std::nullopt};
        if (final_byte == 'Z')
            return {Status::Complete, consumed,
                    TerminalEvent{KeyEvent{KeyChord{Key::Tab, Modifier::Shift, {}}}}, std::nullopt};
        if (final_byte == 'I')
            return {Status::Complete, consumed, TerminalEvent{FocusEvent{true}}, std::nullopt};
        if (final_byte == 'O')
            return {Status::Complete, consumed, TerminalEvent{FocusEvent{false}}, std::nullopt};
    }

    if (private_marker && marker == '<' && params.size() == 3 &&
        (final_byte == 'M' || final_byte == 'm')) {
        const bool is_press = (final_byte == 'M');
        const int cb = params[0];
        const int cx = params[1];
        const int cy = params[2];
        MouseEvent ev;
        const int button_bits = cb & 0x03;
        const bool is_wheel = (cb & 0x40) != 0;
        const bool is_motion = (cb & 0x20) != 0;
        if (is_wheel) {
            ev.button = (button_bits == 0) ? MouseButton::WheelUp : MouseButton::WheelDown;
            ev.action = MouseAction::Wheel;
        } else {
            switch (button_bits) {
                case 0: ev.button = MouseButton::Left; break;
                case 1: ev.button = MouseButton::Middle; break;
                case 2: ev.button = MouseButton::Right; break;
                default: ev.button = MouseButton::None; break;
            }
            ev.action = is_motion ? MouseAction::Move : (is_press ? MouseAction::Down : MouseAction::Up);
        }
        Modifier mod = Modifier::None;
        if (cb & 0x04) mod = mod | Modifier::Shift;
        if (cb & 0x08) mod = mod | Modifier::Alt;
        if (cb & 0x10) mod = mod | Modifier::Ctrl;
        ev.modifiers = mod;

        // Backends/probes are expected to only enable pixel_mouse once
        // cell_pixels is also known — but that's a caller-configuration
        // expectation, not something this decoder can enforce on the
        // bytes it receives, so it degrades gracefully rather than
        // reporting a bogus Point{0,0} (indistinguishable from a real
        // click there) if the expectation is ever violated: with no
        // known cell size, cx/cy are the best available cell-position
        // estimate, exactly like the non-pixel-mouse path.
        if (caps_.pixel_mouse) {
            ev.pixel = PixelPoint{cx - 1, cy - 1};
            if (caps_.cell_pixels.width > 0 && caps_.cell_pixels.height > 0)
                ev.cell = Point{(cx - 1) / caps_.cell_pixels.width, (cy - 1) / caps_.cell_pixels.height};
            else
                ev.cell = Point{cx - 1, cy - 1};
        } else {
            ev.cell = Point{cx - 1, cy - 1};
        }
        return {Status::Complete, consumed, TerminalEvent{ev}, std::nullopt};
    }

    if (private_marker && marker == '?' && final_byte == 'c')
        return {Status::Complete, consumed, std::nullopt, std::nullopt};  // DA1: recognized, not parsed further (v1 scope)

    if (private_marker && marker == '?' && final_byte == 'y') {
        if (params.size() == 2 && params[0] == 2026) {
            Capabilities updated = caps_;
            updated.synchronized_output = (params[1] == 1 || params[1] == 3);
            return {Status::Complete, consumed, std::nullopt, updated};
        }
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    return {Status::Complete, consumed, std::nullopt, std::nullopt};  // unrecognized: swallow whole
}

InputDecoder::ParseResult InputDecoder::parse_osc(std::string_view buf) {
    std::size_t term_pos = std::string_view::npos;
    std::size_t term_len = 0;
    for (std::size_t i = 2; i < buf.size(); ++i) {
        if (static_cast<unsigned char>(buf[i]) == 0x07) {
            term_pos = i;
            term_len = 1;
            break;
        }
        if (buf[i] == 0x1B) {
            if (i + 1 >= buf.size()) return {Status::Incomplete, 0, std::nullopt, std::nullopt};
            if (buf[i + 1] == '\\') {
                term_pos = i;
                term_len = 2;
                break;
            }
        }
    }
    if (term_pos == std::string_view::npos) return {Status::Incomplete, 0, std::nullopt, std::nullopt};

    const std::string_view body = buf.substr(2, term_pos - 2);
    const std::size_t consumed = term_pos + term_len;
    const std::size_t semi = body.find(';');
    if (semi == std::string_view::npos) return {Status::Complete, consumed, std::nullopt, std::nullopt};

    const std::string_view ps_str = body.substr(0, semi);
    const std::string_view pt_str = body.substr(semi + 1);
    int ps = 0;
    for (const char c : ps_str) {
        if (c < '0' || c > '9') {
            ps = -1;
            break;
        }
        ps = ps * 10 + (c - '0');
    }
    if (ps == 11) {
        if (const auto luminance = parse_rgb_luminance(pt_str)) {
            Capabilities updated = caps_;
            updated.color_scheme = (*luminance < 0.5) ? ColorScheme::Dark : ColorScheme::Light;
            return {Status::Complete, consumed, std::nullopt, updated};
        }
    }
    return {Status::Complete, consumed, std::nullopt, std::nullopt};
}

InputDecoder::ParseResult InputDecoder::parse_paste_continuation(std::string_view buf) {
    static constexpr std::string_view kEndMarker = "\x1B[201~";
    const std::size_t end = buf.find(kEndMarker);
    if (end == std::string_view::npos) {
        const std::size_t tail_reserve = kEndMarker.size() - 1;
        const std::size_t safe_len = buf.size() > tail_reserve ? buf.size() - tail_reserve : 0;
        if (safe_len > 0) {
            paste_accum_.append(buf.substr(0, safe_len));
            return {Status::Complete, safe_len, std::nullopt, std::nullopt};
        }
        return {Status::Incomplete, 0, std::nullopt, std::nullopt};
    }
    paste_accum_.append(buf.substr(0, end));
    const std::string sanitized = sanitize_paste(paste_accum_);
    paste_accum_.clear();
    in_paste_ = false;
    return {Status::Complete, end + kEndMarker.size(), TerminalEvent{TextEvent{sanitized, true}},
            std::nullopt};
}

}  // namespace ckv::term
