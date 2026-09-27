// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ckv {

// Named (non-printable or otherwise special) keys. Printable text is
// carried separately on KeyChord, never encoded as a Key value — one
// key model, no dual encodings (the architecture §2).
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
    // The Menu (Application) key: the keyboard's context-menu request, the
    // same request as Shift+F10 (widgets::is_keyboard_context_menu_request).
    // An ordinary key, routed and bindable like F10. Hosts report it as
    // `CSI 29 ~` (the VT220 Do/F16 position the xterm lineage and
    // rxvt-unicode give it) or as kitty's functional key 57363.
    Menu,
    Char,  // a printable character; see KeyChord::text
    // Standalone keys: the modifier keys themselves and the two Super
    // (Windows) keys, each reported as a key in its own right rather than as
    // a bit on some other key's chord. Only a host that reports physical keys
    // (kitty's all-keys-as-escape-codes enhancement) produces them, and
    // Application routes them only to a view that asks
    // (View::accepts_standalone_keys) — never to a command binding. See
    // is_standalone_key() and the decision log D-074.
    LeftShift,
    LeftCtrl,
    LeftAlt,
    LeftSuper,
    RightShift,
    RightCtrl,
    RightAlt,
    RightSuper,
};

// Whether `key` is one of the standalone keys above: a key that is a
// physical event of its own and names no chord an application binds.
constexpr bool is_standalone_key(Key key) noexcept {
    return key >= Key::LeftShift && key <= Key::RightSuper;
}

// Modifier keys held with a key or mouse report, as independent bit flags;
// a Modifier value holds any combination, and None is the empty set. The bit
// order is that of the terminal's modifier parameter (Shift 1, Alt 2, Ctrl 4,
// Super 8), so Super appears only when a host reports that bit.
enum class Modifier : std::uint8_t {
    None = 0,
    Shift = 1u << 0,
    Alt = 1u << 1,
    Ctrl = 1u << 2,
    Super = 1u << 3,
};

// The union of two modifier sets.
constexpr Modifier operator|(Modifier a, Modifier b) noexcept {
    return static_cast<Modifier>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
// Whether `set` contains any flag of `flag`. With a single flag, as intended,
// that is simply "is it held"; Modifier::None is never contained.
constexpr bool has_modifier(Modifier set, Modifier flag) noexcept {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flag)) != 0;
}

// A key press: the named key plus modifiers, and — for `Key::Char` — the
// UTF-8 text it represents. `text` is empty for named keys.
struct KeyChord {
    // The named key (Key::Char for printable text), the modifiers held with
    // it, and the UTF-8 text of a Key::Char chord. A default chord is
    // Key::None with no modifiers, which names no real key.
    Key key = Key::None;
    Modifier modifiers = Modifier::None;
    std::string text;

    // Memberwise equality. The text compares byte for byte, so "g" and "G"
    // are different chords.
    friend bool operator==(const KeyChord&, const KeyChord&) = default;

    // Parses a canonical chord spelling (M9/WP-11), e.g. "Alt+G",
    // "Ctrl+Shift+F10", "Esc", "Enter" — modifier names (any of
    // "Ctrl"/"Alt"/"Shift"/"Super", case-insensitive, in any order,
    // one "+"-joined prefix each) followed by either a name from
    // key_name()'s table, "Space" (the space bar's Key::Char " "), or a
    // single printable character, which becomes a Key::Char chord
    // (lowercased, matching how printable chords are represented
    // everywhere else in the library). Returns
    // nullopt for anything that doesn't parse — an unknown modifier
    // token, an empty or multi-character trailing segment that isn't
    // a known key name, a standalone key (which no binding can ever
    // receive), or an empty string.
    static std::optional<KeyChord> parse(std::string_view text);
};

// The inverse of KeyChord::parse: renders `chord` back to its
// canonical spelling ("Alt+G", "Esc", "F10"). A single-letter
// Key::Char chord is upper-cased for display ("Alt+G", not
// "Alt+g") — purely cosmetic, parse() still accepts either case — and
// the space bar's is spelled "Space" ("Ctrl+Alt+Space"), since a space
// would be invisible in every hint that shows it.
// CKV_ASSERT if chord.key is Key::None (never a real, displayable
// binding) — callers render a chord hint only when one exists.
std::string format(const KeyChord& chord);

// The canonical display name for a named key ("Enter", "Esc", "F10",
// "PageUp", ...). CKV_ASSERT if key is Key::Char or Key::None —
// Key::Char has no fixed name (see KeyChord::text) and Key::None
// never denotes a real key.
std::string_view key_name(Key key) noexcept;

// The inverse of key_name(): case-insensitive lookup of a named key by
// its canonical spelling. nullopt if `name` doesn't match any Key.
std::optional<Key> key_from_name(std::string_view name) noexcept;

}  // namespace ckv
