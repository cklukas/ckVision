// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

namespace ckv {

// Named (non-printable or otherwise special) keys. Printable text is
// carried separately on KeyChord, never encoded as a Key value — one
// key model, no dual encodings (ARCHITECTURE.md §2).
enum class Key : std::uint16_t {
    None = 0,
    Enter,
    Escape,
    Tab,
    Backspace,
    Delete,
    Insert,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Char,  // a printable character; see KeyChord::text
};

enum class Modifier : std::uint8_t {
    None = 0,
    Shift = 1u << 0,
    Alt = 1u << 1,
    Ctrl = 1u << 2,
    Super = 1u << 3,
};

constexpr Modifier operator|(Modifier a, Modifier b) noexcept {
    return static_cast<Modifier>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr bool has_modifier(Modifier set, Modifier flag) noexcept {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flag)) != 0;
}

// A key press: the named key plus modifiers, and — for `Key::Char` — the
// UTF-8 text it represents. `text` is empty for named keys.
struct KeyChord {
    Key key = Key::None;
    Modifier modifiers = Modifier::None;
    std::string text;

    friend bool operator==(const KeyChord&, const KeyChord&) = default;
};

}  // namespace ckv
