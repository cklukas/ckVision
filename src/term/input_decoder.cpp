// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Coverage and every scope decision: docs/input-decoder.md.
#include "cvision/term/input_decoder.hpp"

#include <algorithm>
#include <climits>
#include <iterator>
#include <optional>

#include "cvision/core/utf8.hpp"
#include "cvision/term/pointer_shape_names.hpp"

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

// A CSI parameter list, with each parameter's colon-separated subparameters
// kept in place. kitty's key grammar gives every position a default and an
// empty position selects it, so "missing" must stay distinguishable from a
// real zero: an absent or empty position is kCsiParamMissing.
constexpr int kCsiParamMissing = -1;

std::vector<std::vector<int>> parse_csi_parameters(std::string_view s) {
    std::vector<std::vector<int>> fields;
    if (s.empty()) return fields;
    std::vector<int> current;
    int value = 0;
    bool any_digit = false;
    const auto flush_sub = [&] {
        current.push_back(any_digit ? value : kCsiParamMissing);
        value = 0;
        any_digit = false;
    };
    for (const char c : s) {
        if (c == ';') {
            flush_sub();
            fields.push_back(std::move(current));
            current.clear();
            continue;
        }
        if (c == ':') {
            flush_sub();
            continue;
        }
        if (c < '0' || c > '9') continue;
        any_digit = true;
        // Saturating accumulation: a hostile or malformed sequence can
        // supply an arbitrarily long digit run (CSI/OSC parameters are
        // conventionally a few digits at most), and naive
        // value*10+digit overflow on signed int is undefined behavior,
        // not just wraparound — confirmed reachable and UBSan-flagged
        // for a real overlong parameter before this fix.
        if (value > (INT_MAX - (c - '0')) / 10) {
            value = INT_MAX;
            continue;
        }
        value = value * 10 + (c - '0');
    }
    flush_sub();
    fields.push_back(std::move(current));
    return fields;
}

// One position of the structured parameter list, with its grammar default.
int csi_field(const std::vector<std::vector<int>>& fields, std::size_t field, std::size_t sub,
              int fallback) noexcept {
    if (field >= fields.size() || sub >= fields[field].size()) return fallback;
    const int value = fields[field][sub];
    return value == kCsiParamMissing ? fallback : value;
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

// kitty's event-type subparameter, riding second in the modifiers field of
// every escape-coded key — the legacy-form functional keys included:
// `CSI 1;1:3 A` is Up going back up, not a second Up. Press is the omitted
// default; an unknown transition is rejected rather than degraded into an
// activation.
std::optional<KeyAction> key_action_from_event_type(int event_type) noexcept {
    switch (event_type) {
        case 1: return KeyAction::Press;
        case 2: return KeyAction::Repeat;
        case 3: return KeyAction::Release;
        default: return std::nullopt;
    }
}

// Whether this session's escape-coded keys will report their release: the
// kitty protocol with its verified event-type enhancement. Keys still on
// legacy encodings under the same session (possible whenever the
// all-keys-as-escape-codes enhancement is not in force) never arrive here,
// so the per-event flag stays honest for them by construction.
bool session_reports_escape_coded_releases(const Capabilities& caps) noexcept {
    return caps.keyboard_protocol == KeyboardProtocol::Kitty &&
           (caps.kitty_keyboard_flags & kKittyReportEventTypes) != 0;
}

// kitty's functional keys occupy the Unicode private-use block 57344-63743.
// The ones ckVision's key model names are mapped here; the keypad's
// character keys carry a canonical character so a numeric keypad still
// types under the all-keys-as-escape-codes enhancement; every other code in
// the block — the modifier and lock keys, the media keys, F13 and beyond —
// is a real key this model does not name, consumed deliberately rather than
// delivered to text controls as private-use garbage.
constexpr int kKittyFunctionalFirst = 57344;
constexpr int kKittyFunctionalLast = 63743;

std::optional<Key> kitty_functional_key(int code) noexcept {
    switch (code) {
        case 57414: return Key::Enter;  // KP_ENTER: an Enter wherever it sits
        case 57417: return Key::Left;
        case 57418: return Key::Right;
        case 57419: return Key::Up;
        case 57420: return Key::Down;
        case 57421: return Key::PageUp;
        case 57422: return Key::PageDown;
        case 57423: return Key::Home;
        case 57424: return Key::End;
        case 57425: return Key::Insert;
        case 57426: return Key::Delete;
        default: return std::nullopt;
    }
}

std::string_view kitty_keypad_text(int code) noexcept {
    switch (code) {
        case 57399: return "0";
        case 57400: return "1";
        case 57401: return "2";
        case 57402: return "3";
        case 57403: return "4";
        case 57404: return "5";
        case 57405: return "6";
        case 57406: return "7";
        case 57407: return "8";
        case 57408: return "9";
        case 57409: return ".";
        case 57410: return "/";
        case 57411: return "*";
        case 57412: return "-";
        case 57413: return "+";
        case 57415: return "=";
        case 57416: return ",";
        default: return {};
    }
}

// The associated-text field (kitty's text-as-codepoints): what the key
// actually produced, which is the only correct source for a shifted or
// composed character once keys stop arriving as text. Sanitized the way
// chord text must be — control, C1 and non-scalar values never become key
// text, whatever a host or a hostile stream embeds in the field.
std::string decode_kitty_text(const std::vector<std::vector<int>>& fields, std::size_t index) {
    std::string text;
    if (index >= fields.size()) return text;
    for (const int cp : fields[index]) {
        if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F)) continue;
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) continue;
        utf8::encode(static_cast<char32_t>(cp), text);
    }
    return text;
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

// D-040's paste sanitization rule — distinct from
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

// "1,0,1,1,…" -> a bit per PointerShape in enum order, or nullopt when the
// payload is not an answer to the support query.
//
// The strictness is the point. OSC 22 is one code for a question and its
// answer, and a host may also send back a shape NAME (that is what the
// `?__current__` form asks for, which ckVision does not use). Insisting on
// a payload made only of single flags and commas is what keeps a name from
// being read as a support map — `0` alone, the reply meaning "no shape is
// set", is correctly refused here rather than taken as a one-shape answer.
std::optional<std::uint32_t> parse_pointer_shape_support(std::string_view payload) {
    if (payload.empty()) return std::nullopt;
    std::uint32_t supported = 0;
    int index = 0;
    std::size_t pos = 0;
    while (true) {
        const std::size_t comma = payload.find(',', pos);
        const std::string_view field =
            comma == std::string_view::npos ? payload.substr(pos) : payload.substr(pos, comma - pos);
        if (field.size() != 1 || (field[0] != '0' && field[0] != '1')) return std::nullopt;
        if (index >= kPointerShapeCount) return std::nullopt;
        if (field[0] == '1') supported |= std::uint32_t{1} << index;
        ++index;
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    // A single flag is a name reply ("0" for an empty stack), not a support
    // map: the query named every shape, so a real answer enumerates them.
    if (index < 2) return std::nullopt;
    return supported;
}

}  // namespace

std::vector<TerminalEvent> InputDecoder::feed(std::string_view data, std::int64_t now_nanos) {
    std::vector<TerminalEvent> events;
    if (auto completed = finish_paste_if_quiet(now_nanos)) events.push_back(std::move(*completed));
    pending_.append(data);
    std::vector<TerminalEvent> decoded = drain(now_nanos);
    events.insert(events.end(), std::make_move_iterator(decoded.begin()), std::make_move_iterator(decoded.end()));
    return events;
}

std::vector<TerminalEvent> InputDecoder::poll_timeout(std::int64_t now_nanos) {
    std::vector<TerminalEvent> events = drain(now_nanos);
    if (auto completed = finish_paste_if_quiet(now_nanos)) events.push_back(std::move(*completed));
    return events;
}

std::vector<TerminalEvent> InputDecoder::abort_paste() {
    std::vector<TerminalEvent> events;
    if (auto recovered = abort_active_paste()) events.push_back(std::move(*recovered));
    pending_.clear();
    esc_first_seen_nanos_ = -1;
    discarding_unnegotiated_paste_ = false;
    return events;
}

std::optional<std::int64_t> InputDecoder::next_timeout_nanos() const noexcept {
    if (!in_paste_ || !paste_end_candidate_) return std::nullopt;
    return paste_end_candidate_nanos_ + kPasteTerminationQuietNanos;
}

std::vector<TerminalEvent> InputDecoder::drain(std::int64_t now_nanos) {
    std::vector<TerminalEvent> events;
    while (!pending_.empty()) {
        const ParseResult r = try_parse_one(pending_, now_nanos);
        if (r.status == Status::Incomplete) break;
        if (r.status == Status::Invalid) {
            pending_.erase(0, 1);
            discard_capability_update_from_pending_sequence_ = false;
            continue;
        }
        const bool crossed_probe_window = discard_capability_update_from_pending_sequence_;
        if (crossed_probe_window) {
            // Some capability parsers retain auxiliary evidence while
            // constructing their candidate update (notably DECRPM 1016's
            // mode proof). The candidate is stale by construction, so reset
            // every such parser-local fact to this window's accepted profile
            // before processing later fresh replies.
            set_capabilities(caps_);
        } else if (r.updated_caps) {
            const bool needs_fresh_geometry = sixel_geometry_required_ && r.updated_caps->sixel_graphics &&
                                              (r.updated_caps->sixel_max_geometry.width <= 0 ||
                                               r.updated_caps->sixel_max_geometry.height <= 0);
            if (accepts_capability_update(*r.updated_caps) && !needs_fresh_geometry) {
                if (*r.updated_caps != caps_) {
                    caps_ = *r.updated_caps;
                    events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
                }
                if (sixel_geometry_required_ && caps_.sixel_graphics) sixel_geometry_required_ = false;
            } else if (!needs_fresh_geometry) {
                // Parsing a capability reply may update auxiliary evidence
                // before producing its candidate (notably DECRPM 1016). A
                // backend that rejects this reply must restore all decoder
                // state now, before the next sequence from the same feed.
                set_capabilities(caps_);
            } else {
                // DA1 is valid positive evidence at session start, but after
                // a resize its lack of a current geometry tells us nothing
                // about the new safe image bound. Consume it without changing
                // unrelated decoder state; a following XTSMGRAPHICS geometry
                // reply in this same read remains authoritative.
            }
        }
        if (r.event) events.push_back(std::move(*r.event));
        pending_.erase(0, r.consumed);
        discard_capability_update_from_pending_sequence_ = false;
    }
    return events;
}

bool InputDecoder::accepts_capability_update(const Capabilities& candidate) const noexcept {
    switch (capability_update_policy_) {
        case CapabilityUpdatePolicy::AcceptProbeRefinements:
            return true;
        case CapabilityUpdatePolicy::Reject: {
            // A cell metric is a measurement, not a negotiated capability:
            // it cannot be spoofed into enabling anything, and a terminal
            // that answers XTWINOPS after the probe window still answers
            // truthfully. Refusing it strands pixel-coordinate mouse input
            // with nothing to divide by. Accept a candidate that differs
            // only in geometry; reject everything else as before.
            Capabilities geometry_only = candidate;
            geometry_only.cell_pixels = caps_.cell_pixels;
            geometry_only.text_area_pixels = caps_.text_area_pixels;
            return geometry_only == caps_;
        }
        case CapabilityUpdatePolicy::AcceptVerifiedLiveRefinements: {
            // A cell metric is a measurement, admissible here for the same
            // reason the Reject policy admits it below.
            {
                Capabilities geometry_only = candidate;
                geometry_only.cell_pixels = caps_.cell_pixels;
                geometry_only.text_area_pixels = caps_.text_area_pixels;
                if (geometry_only == caps_) return true;
            }
            // A kitty flag readback can land after the probe window closed:
            // the backend queries again whenever it pushes or demotes the
            // enhancement set. Once the protocol itself was verified inside
            // a window, a reply that changes nothing but the flag record is
            // accepted for the session's lifetime — the same standing the
            // verified 2031 notifications have below. The query is only
            // ever solicited by this side, and the record enables no
            // decoding a spoofed value could exploit: decode follows the
            // shape of what arrives, the flags only set what a key promises.
            if (caps_.keyboard_protocol == KeyboardProtocol::Kitty &&
                candidate.kitty_keyboard_flags != caps_.kitty_keyboard_flags) {
                Capabilities flags_only = candidate;
                flags_only.kitty_keyboard_flags = caps_.kitty_keyboard_flags;
                if (flags_only == caps_) return true;
            }
            if (!caps_.color_scheme_notifications || !candidate.color_scheme_notifications ||
                candidate.color_scheme == caps_.color_scheme)
                return false;
            Capabilities without_scheme = candidate;
            without_scheme.color_scheme = caps_.color_scheme;
            return without_scheme == caps_;
        }
    }
    return false;
}

InputDecoder::ParseResult InputDecoder::try_parse_one(std::string_view buf, std::int64_t now_nanos) {
    if (in_paste_ || discarding_unnegotiated_paste_)
        return parse_paste_continuation(buf, now_nanos);
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
        // Kitty's negotiated keyboard protocol encodes Escape itself as a
        // CSI-u key event, so a bare ESC cannot be an Alt prefix. Deliver it
        // immediately rather than injecting the legacy quiet-delay into a
        // protocol that explicitly removes that ambiguity.
        if (caps_.keyboard_protocol == KeyboardProtocol::Kitty)
            return {Status::Complete, 1,
                    TerminalEvent{KeyEvent{KeyChord{Key::Escape, Modifier::None, {}}}}, std::nullopt};
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
    // X10's mouse report is a six-byte CSI sequence whose three payload bytes
    // are not CSI parameters. Recognize it before generic CSI parsing, both
    // so an authorized X10 profile receives the press and so every other
    // profile swallows the unsupported extension as one unit rather than
    // leaking its coordinate bytes into normal text input.
    if (buf.size() >= 3 && buf[2] == 'M') {
        if (buf.size() < 6) return {Status::Incomplete, 0, std::nullopt, std::nullopt};
        if (caps_.mouse_protocol != MouseProtocol::X10)
            return {Status::Complete, 6, std::nullopt, std::nullopt};

        const int button_code = static_cast<unsigned char>(buf[3]) - 32;
        const int x = static_cast<unsigned char>(buf[4]) - 32;
        const int y = static_cast<unsigned char>(buf[5]) - 32;
        if (button_code < 0 || button_code > 2 || x < 1 || y < 1)
            return {Status::Complete, 6, std::nullopt, std::nullopt};

        const MouseButton button = button_code == 0   ? MouseButton::Left
                                   : button_code == 1 ? MouseButton::Middle
                                                      : MouseButton::Right;
        return {Status::Complete, 6,
                TerminalEvent{MouseEvent{MouseAction::Down, button, Point{x - 1, y - 1}, std::nullopt,
                                         Modifier::None}},
                std::nullopt};
    }

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
        param_str += buf[pos];
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
    const std::vector<std::vector<int>> fields = parse_csi_parameters(param_str);
    // The flat view every subparameter-free consumer reads: one value per
    // top-level parameter, an empty position reading as the conventional 0.
    // The key paths that negotiated kitty's grammar read `fields` instead.
    std::vector<int> params;
    params.reserve(fields.size());
    for (const std::vector<int>& field : fields)
        params.push_back(field.empty() || field.front() == kCsiParamMissing ? 0 : field.front());

    if (final_byte == '~') {
        if (params.size() >= 1 && params[0] == 200) {
            if (caps_.bracketed_paste) {
                in_paste_ = true;
                paste_accum_.clear();
                paste_candidate_tail_.clear();
                paste_end_candidate_ = false;
                paste_end_candidate_nanos_ = -1;
                paste_recovered_ = false;
            } else {
                // A profile that did not request bracketed paste must not let
                // a hostile or misconfigured terminal turn the bytes between
                // its delimiters into ordinary key/text input. Preserve the
                // stream boundary but discard it as opaque unsupported input.
                discarding_unnegotiated_paste_ = true;
            }
            return {Status::Complete, consumed, std::nullopt, std::nullopt};
        }
        if (params.size() >= 1 && params[0] == 201)
            return {Status::Complete, consumed, std::nullopt, std::nullopt};  // stray end marker
        if (caps_.keyboard_protocol == KeyboardProtocol::ModifyOtherKeys && params.size() == 3 &&
            params[0] == 27) {
            const Modifier mod = decode_modifier_param(params[1]);
            return {Status::Complete, consumed, TerminalEvent{KeyEvent{key_from_codepoint(params[2], mod)}},
                    std::nullopt};
        }
        if (!params.empty()) {
            const Modifier mod = decode_modifier_param(csi_field(fields, 1, 0, 1));
            const int event_type = caps_.keyboard_protocol == KeyboardProtocol::Kitty
                                       ? csi_field(fields, 1, 1, 1)
                                       : 1;
            const auto action = key_action_from_event_type(event_type);
            if (!action) return {Status::Complete, consumed, std::nullopt, std::nullopt};
            if (const auto key = tilde_key_from_number(params[0]))
                return {Status::Complete, consumed,
                        TerminalEvent{KeyEvent{KeyChord{*key, mod, {}}, *action,
                                               session_reports_escape_coded_releases(caps_)}},
                        std::nullopt};
        }
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    if (final_byte == 'u' && !private_marker && caps_.keyboard_protocol == KeyboardProtocol::Kitty) {
        // The protocol's one escape code for key events:
        //   CSI key[:alternates] ; modifiers[:event] ; text u
        // The alternate-key codes are recognized and deliberately not read —
        // the input model carries a key and the text it produced, never the
        // keyboard layout behind them (D-047). The text field IS read:
        // once keys stop arriving as text, it is the only correct source
        // for a shifted or composed character, and guessing it from the key
        // number would be exactly the layout modelling this library refuses.
        if (params.empty()) return {Status::Complete, consumed, std::nullopt, std::nullopt};
        const int key_code = csi_field(fields, 0, 0, 0);
        if (key_code <= 0) return {Status::Complete, consumed, std::nullopt, std::nullopt};
        const Modifier mod = decode_modifier_param(csi_field(fields, 1, 0, 1));
        const auto action = key_action_from_event_type(csi_field(fields, 1, 1, 1));
        if (!action) return {Status::Complete, consumed, std::nullopt, std::nullopt};
        const bool releases = session_reports_escape_coded_releases(caps_);
        if (key_code >= kKittyFunctionalFirst && key_code <= kKittyFunctionalLast) {
            if (const auto key = kitty_functional_key(key_code))
                return {Status::Complete, consumed,
                        TerminalEvent{KeyEvent{KeyChord{*key, mod, {}}, *action, releases}},
                        std::nullopt};
            std::string text = decode_kitty_text(fields, 2);
            if (text.empty()) text = std::string(kitty_keypad_text(key_code));
            // A functional key the model does not name is consumed, never
            // leaked as private-use text. If it produced real text — the
            // keypad's character keys always do — the text is the event;
            // its release pairs with nothing this model names, so such an
            // event never promises one.
            if (text.empty() || *action == KeyAction::Release)
                return {Status::Complete, consumed, std::nullopt, std::nullopt};
            return {Status::Complete, consumed,
                    TerminalEvent{KeyEvent{KeyChord{Key::Char, mod, std::move(text)}, *action,
                                           false}},
                    std::nullopt};
        }
        KeyChord chord = key_from_codepoint(key_code, mod);
        if (chord.key == Key::Char) {
            std::string text = decode_kitty_text(fields, 2);
            if (!text.empty()) chord.text = std::move(text);
        }
        return {Status::Complete, consumed,
                TerminalEvent{KeyEvent{std::move(chord), *action, releases}}, std::nullopt};
    }

    if (!private_marker) {
        const Modifier mod = decode_modifier_param(csi_field(fields, 1, 0, 1));
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
        if (key) {
            const int event_type = caps_.keyboard_protocol == KeyboardProtocol::Kitty
                                       ? csi_field(fields, 1, 1, 1)
                                       : 1;
            const auto action = key_action_from_event_type(event_type);
            if (!action) return {Status::Complete, consumed, std::nullopt, std::nullopt};
            return {Status::Complete, consumed,
                    TerminalEvent{KeyEvent{KeyChord{*key, mod, {}}, *action,
                                           session_reports_escape_coded_releases(caps_)}},
                    std::nullopt};
        }
        if (final_byte == 'Z')
            return {Status::Complete, consumed,
                    TerminalEvent{KeyEvent{KeyChord{Key::Tab, Modifier::Shift, {}}}}, std::nullopt};
        if (final_byte == 'I' && caps_.focus_events)
            return {Status::Complete, consumed, TerminalEvent{FocusEvent{true}}, std::nullopt};
        if (final_byte == 'O' && caps_.focus_events)
            return {Status::Complete, consumed, TerminalEvent{FocusEvent{false}}, std::nullopt};
    }

    if (caps_.mouse_protocol == MouseProtocol::SGR && private_marker && marker == '<' && params.size() == 3 &&
        (final_byte == 'M' || final_byte == 'm')) {
        ++mouse_reports_seen_;
        const bool is_press = (final_byte == 'M');
        const int cb = params[0];
        const int cx = params[1];
        const int cy = params[2];
        // A 1-based cell coordinate can never exceed the grid the terminal
        // itself renders, so a report beyond it is pixel data no matter
        // what DECRPM claimed: iTerm2 answers mode 1016 as "permanently
        // reset" and then engages it anyway. The reports are the authority
        // on what the terminal is actually sending, and the evidence is
        // recorded even when the report itself is consumed below.
        const bool beyond_grid = cell_grid_.width > 0 && cell_grid_.height > 0 &&
                                 (cx > cell_grid_.width || cy > cell_grid_.height);
        if (beyond_grid) pixel_mouse_mode_enabled_ = true;
        // SGR-pixel mode changes the meaning of x/y before the backend can
        // safely expose it: mode 1016 is enabled while probing, but a
        // DECRPM reply alone does not provide the cell metric. Do not guess
        // that these are ordinary cell coordinates in that interval.
        if (sgr_mouse_input_suppressed_ && !caps_.pixel_mouse)
            return {Status::Complete, consumed, std::nullopt, std::nullopt};
        MouseEvent ev;
        const int button_bits = cb & 0x03;
        const bool is_wheel = (cb & 0x40) != 0;
        const bool is_motion = (cb & 0x20) != 0;
        if (is_wheel) {
            switch (button_bits) {
                case 0: ev.button = MouseButton::WheelUp; break;
                case 1: ev.button = MouseButton::WheelDown; break;
                case 2: ev.button = MouseButton::WheelLeft; break;
                default: ev.button = MouseButton::WheelRight; break;
            }
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
        if (caps_.pixel_mouse || beyond_grid) {
            ev.pixel = PixelPoint{cx - 1, cy - 1};
            if (caps_.cell_pixels.width > 0 && caps_.cell_pixels.height > 0) {
                ev.cell = Point{(cx - 1) / caps_.cell_pixels.width, (cy - 1) / caps_.cell_pixels.height};
            } else if (caps_.text_area_pixels.width > 0 && caps_.text_area_pixels.height > 0 &&
                       cell_grid_.width > 0 && cell_grid_.height > 0) {
                // No direct metric, but the text area and the grid give one.
                const int cw = std::max(1, caps_.text_area_pixels.width / cell_grid_.width);
                const int ch = std::max(1, caps_.text_area_pixels.height / cell_grid_.height);
                ev.cell = Point{(cx - 1) / cw, (cy - 1) / ch};
            } else if (caps_.pixel_mouse) {
                ev.cell = Point{cx - 1, cy - 1};
            } else {
                // Pixel evidence and nothing to map it with: a fabricated
                // cell would land the click on an arbitrary control.
                return {Status::Complete, consumed, std::nullopt, std::nullopt};
            }
        } else {
            ev.cell = Point{cx - 1, cy - 1};
        }
        return {Status::Complete, consumed, TerminalEvent{ev}, std::nullopt};
    }

    if (private_marker && marker == '?' && final_byte == 'u') {
        // The kitty keyboard protocol answers CSI ? <flags> u to CSI ? u.
        // Any answer at all proves the protocol exists — a terminal without
        // it stays silent. The value is the enhancement set actually in
        // force: read back after the backend pushes what it wants, it is
        // the record of what was honoured rather than of what was asked
        // (D-047 from the client side). Masked to the requestable set so a
        // host volunteering alternate-key codes cannot make this session
        // claim an enhancement it never requests.
        Capabilities updated = caps_;
        updated.keyboard_protocol = KeyboardProtocol::Kitty;
        updated.kitty_keyboard_flags = (params.empty() ? 0 : params[0]) & kKittyRequestedFlags;
        if (updated == caps_) return {Status::Complete, consumed, std::nullopt, std::nullopt};
        return {Status::Complete, consumed, std::nullopt, updated};
    }

    if (private_marker && marker == '?' && final_byte == 'c') {
        // Primary Device Attributes reports parameter 4 for Sixel graphics
        // on DEC graphics terminals and xterm. This is positive evidence
        // only: a terminal may omit the parameter despite a curated host
        // profile knowing more, so DA1 never disables an already-forced
        // graphics capability.
        const bool advertises_sixel = std::find(params.begin(), params.end(), 4) != params.end();
        if (advertises_sixel && !caps_.sixel_graphics) {
            Capabilities updated = caps_;
            updated.sixel_graphics = true;
            return {Status::Complete, consumed, std::nullopt, updated};
        }
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    if (private_marker && marker == '?' && final_byte == 'S') {
        // XTSMGRAPHICS replies CSI ? Pi ; Ps ; Pv S. Pi=1 reports the
        // Sixel color-register maximum; Pi=2 reports Sixel geometry in
        // pixels. Ps=0 is the only successful result. A successful geometry
        // reply is independent positive evidence that Sixel is configured.
        if (params.size() >= 3 && params[1] == 0) {
            Capabilities updated = caps_;
            if (params[0] == 1 && params.size() == 3 && params[2] >= 0) {
                updated.sixel_color_registers = params[2];
                return {Status::Complete, consumed, std::nullopt, updated};
            }
            if (params[0] == 2 && params.size() == 4 && params[2] > 0 && params[3] > 0) {
                updated.sixel_graphics = true;
                updated.sixel_max_geometry = Size{params[2], params[3]};
                return {Status::Complete, consumed, std::nullopt, updated};
            }
        }
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    if (private_marker && marker == '?' && final_byte == 'y') {
        if (params.size() == 2 && params[0] == 2031) {
            // We enable DECSET 2031 before asking DECRQM. A set or
            // permanently-set report therefore proves that this session can
            // receive the documented unsolicited color-preference DSRs.
            Capabilities updated = caps_;
            updated.color_scheme_notifications = (params[1] == 1 || params[1] == 3);
            return {Status::Complete, consumed, std::nullopt, updated};
        }
        if (params.size() == 2 && params[0] == 2026) {
            Capabilities updated = caps_;
            updated.synchronized_output = (params[1] == 1 || params[1] == 3);
            return {Status::Complete, consumed, std::nullopt, updated};
        }
        if (params.size() == 2 && params[0] == 1016) {
            // The DECRPM reply proves that the SGR-pixel mode is active, but
            // it does not give the pixel-to-cell mapping.  Preserve that
            // fact until XTWINOPS 16 supplies a positive cell size; the two
            // replies have no required delivery order.
            pixel_mouse_mode_enabled_ = (params[1] == 1 || params[1] == 3);
            Capabilities updated = caps_;
            updated.pixel_mouse = pixel_mouse_mode_enabled_ && updated.cell_pixels.width > 0 &&
                                  updated.cell_pixels.height > 0;
            return {Status::Complete, consumed, std::nullopt, updated};
        }
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    // DSR-OK: the answer to `CSI 5 n`, and the only reply this decoder both
    // asks for and consumes without telling anybody. It is not input and it
    // is not a capability — it is the terminal saying "I have read this
    // far" — so it is counted here rather than turned into an event a view
    // tree would have to learn to ignore.
    if (!private_marker && final_byte == 'n' && params.size() == 1 && params[0] == 0) {
        ++frame_acknowledgements_;
        return {Status::Complete, consumed, std::nullopt, std::nullopt};
    }

    if (private_marker && marker == '?' && final_byte == 'n' && params.size() == 2 && params[0] == 997 &&
        (params[1] == 1 || params[1] == 2)) {
        // Mode 2031 is the authority for this otherwise unsolicited DSR.
        // Do not let the notification itself manufacture that authority:
        // direct/headless decoding has no backend probe window to filter it.
        if (!caps_.color_scheme_notifications) {
            return {Status::Complete, consumed, std::nullopt, std::nullopt};
        }
        Capabilities updated = caps_;
        updated.color_scheme = (params[1] == 1) ? ColorScheme::Dark : ColorScheme::Light;
        return {Status::Complete, consumed, std::nullopt, updated};
    }

    if (!private_marker && final_byte == 't' && params.size() == 3 && params[0] == 6 &&
        params[1] > 0 && params[2] > 0) {
        // XTWINOPS 16 replies CSI 6 ; height ; width t.  Size uses the
        // library's conventional width/height ordering, hence [2], [1].
        Capabilities updated = caps_;
        updated.cell_pixels = Size{params[2], params[1]};
        updated.pixel_mouse = pixel_mouse_mode_enabled_;
        return {Status::Complete, consumed, std::nullopt, updated};
    }

    if (!private_marker && final_byte == 't' && params.size() == 3 && params[0] == 4 &&
        params[1] > 0 && params[2] > 0) {
        // XTWINOPS 14 replies CSI 4 ; height ; width t with the text area's
        // total pixel size. When XTWINOPS 16 went unanswered — iTerm2
        // answers 14 but not 16 — the area divided by the cell grid IS the
        // cell metric: the same numbers the terminal itself uses to place
        // glyphs and images. A direct 16 reply keeps precedence in either
        // arrival order: it overwrites, and this derivation defers.
        Capabilities updated = caps_;
        updated.text_area_pixels = Size{params[2], params[1]};
        if ((updated.cell_pixels.width <= 0 || updated.cell_pixels.height <= 0) &&
            cell_grid_.width > 0 && cell_grid_.height > 0) {
            const int cell_width = params[2] / cell_grid_.width;
            const int cell_height = params[1] / cell_grid_.height;
            if (cell_width > 0 && cell_height > 0) {
                updated.cell_pixels = Size{cell_width, cell_height};
                updated.pixel_mouse = pixel_mouse_mode_enabled_;
            }
        }
        return {Status::Complete, consumed, std::nullopt, updated};
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
        if (ps > (INT_MAX - (c - '0')) / 10) {
            ps = -1;
            break;
        }
        ps = ps * 10 + (c - '0');
    }
    // A host answering the pointer-shape support query has told us two
    // things at once: which shapes it has, and — by answering at all — that
    // it implements the specification the CSS names come from. Only a host
    // that got the question can answer it, so nothing here needs to guess.
    if (ps == 22 && caps_.pointer_shapes) {
        if (const auto supported = parse_pointer_shape_support(pt_str)) {
            Capabilities updated = caps_;
            updated.pointer_shape_vocabulary = PointerShapeVocabulary::Standard;
            updated.pointer_shapes_supported = *supported;
            return {Status::Complete, consumed, std::nullopt, updated};
        }
    }
    // Once DEC mode 2031 is established, its explicit dark/light preference
    // is authoritative. Do not let delayed OSC 10/11 replies replace it.
    if (ps == 11 && !caps_.color_scheme_notifications) {
        if (const auto luminance = parse_rgb_luminance(pt_str)) {
            Capabilities updated = caps_;
            updated.color_scheme = (*luminance < 0.5) ? ColorScheme::Dark : ColorScheme::Light;
            color_scheme_from_osc11_ = true;
            return {Status::Complete, consumed, std::nullopt, updated};
        }
    }
    if (ps == 10 && !caps_.color_scheme_notifications && !color_scheme_from_osc11_) {
        if (const auto luminance = parse_rgb_luminance(pt_str)) {
            // OSC 10 reports the default foreground, not a color-scheme
            // preference. Its contrast suggests the inverse background mode
            // only when OSC 11 is unavailable; a later OSC 11 report replaces
            // this lower-confidence fallback.
            Capabilities updated = caps_;
            updated.color_scheme = (*luminance < 0.5) ? ColorScheme::Light : ColorScheme::Dark;
            return {Status::Complete, consumed, std::nullopt, updated};
        }
    }
    return {Status::Complete, consumed, std::nullopt, std::nullopt};
}

InputDecoder::ParseResult InputDecoder::parse_paste_continuation(std::string_view buf,
                                                                  std::int64_t now_nanos) {
    static constexpr std::string_view kEndMarker = "\x1B[201~";
    const std::size_t end = buf.find(kEndMarker);
    if (!in_paste_) {
        if (end == std::string_view::npos) {
            const std::size_t tail_reserve = kEndMarker.size() - 1;
            const std::size_t safe_len = buf.size() > tail_reserve ? buf.size() - tail_reserve : 0;
            return safe_len == 0 ? ParseResult{Status::Incomplete, 0, std::nullopt, std::nullopt}
                                 : ParseResult{Status::Complete, safe_len, std::nullopt, std::nullopt};
        }
        discarding_unnegotiated_paste_ = false;
        return {Status::Complete, end + kEndMarker.size(), std::nullopt, std::nullopt};
    }

    const auto append_data = [this](std::string_view text) {
        if (paste_end_candidate_)
            paste_candidate_tail_.append(text);
        else
            paste_accum_.append(text);
    };

    if (end != std::string_view::npos) {
        if (paste_end_candidate_) {
            // A newer candidate proves the old marker was literal paste data.
            paste_accum_.append(kEndMarker);
            paste_accum_.append(paste_candidate_tail_);
            paste_candidate_tail_.clear();
            paste_recovered_ = true;
        }
        paste_end_candidate_ = false;
        append_data(buf.substr(0, end));
        paste_end_candidate_ = true;
        paste_end_candidate_nanos_ = now_nanos;
        return {Status::Complete, end + kEndMarker.size(), std::nullopt, std::nullopt};
    }

    const std::size_t tail_reserve = kEndMarker.size() - 1;
    const std::size_t safe_len = buf.size() > tail_reserve ? buf.size() - tail_reserve : 0;
    if (safe_len == 0) return {Status::Incomplete, 0, std::nullopt, std::nullopt};
    append_data(buf.substr(0, safe_len));
    if (paste_end_candidate_) paste_recovered_ = true;
    return {Status::Complete, safe_len, std::nullopt, std::nullopt};
}

std::optional<TerminalEvent> InputDecoder::finish_paste_if_quiet(std::int64_t now_nanos) {
    if (!in_paste_ || !paste_end_candidate_ ||
        now_nanos - paste_end_candidate_nanos_ < kPasteTerminationQuietNanos)
        return std::nullopt;

    // A partial next marker intentionally remains buffered by drain(). Once
    // the current candidate's guard elapsed, that partial sequence belongs to
    // the recovered paste too; leaving it for normal escape decoding would
    // reopen the very command-injection path this guard closes.
    if (!pending_.empty()) {
        paste_candidate_tail_.append(pending_);
        pending_.clear();
        paste_recovered_ = true;
    }
    std::string recovered = std::move(paste_accum_);
    recovered += paste_candidate_tail_;
    const bool was_recovered = paste_recovered_ || !paste_candidate_tail_.empty();
    paste_accum_.clear();
    paste_candidate_tail_.clear();
    in_paste_ = false;
    paste_end_candidate_ = false;
    paste_end_candidate_nanos_ = -1;
    paste_recovered_ = false;
    return TerminalEvent{TextEvent{sanitize_paste(recovered), true, was_recovered}};
}

std::optional<TerminalEvent> InputDecoder::abort_active_paste() {
    if (!in_paste_) return std::nullopt;
    if (!pending_.empty()) {
        if (paste_end_candidate_)
            paste_candidate_tail_.append(pending_);
        else
            paste_accum_.append(pending_);
        pending_.clear();
    }
    std::string recovered = std::move(paste_accum_);
    recovered += paste_candidate_tail_;
    paste_accum_.clear();
    paste_candidate_tail_.clear();
    in_paste_ = false;
    paste_end_candidate_ = false;
    paste_end_candidate_nanos_ = -1;
    paste_recovered_ = false;
    return TerminalEvent{TextEvent{sanitize_paste(recovered), true, true}};
}

}  // namespace ckv::term
