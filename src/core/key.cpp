// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/key.hpp"

#include <array>
#include <cctype>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"

namespace ckv {

namespace {

struct NamedKey {
    Key key;
    std::string_view name;
};

// Canonical spelling table (M9/WP-11) — the single source of truth
// for both key_name() and key_from_name(). "Esc", not "Escape":
// every existing comment and test name in this codebase already
// spells it that way (grep the repo — "Esc" appears dozens of times,
// "Escape" only as the enumerator name itself).
constexpr std::array<NamedKey, 26> kNamedKeys{{
    {Key::Enter, "Enter"},
    {Key::Escape, "Esc"},
    {Key::Tab, "Tab"},
    {Key::Backspace, "Backspace"},
    {Key::Delete, "Delete"},
    {Key::Insert, "Insert"},
    {Key::Up, "Up"},
    {Key::Down, "Down"},
    {Key::Left, "Left"},
    {Key::Right, "Right"},
    {Key::Home, "Home"},
    {Key::End, "End"},
    {Key::PageUp, "PageUp"},
    {Key::PageDown, "PageDown"},
    {Key::F1, "F1"},
    {Key::F2, "F2"},
    {Key::F3, "F3"},
    {Key::F4, "F4"},
    {Key::F5, "F5"},
    {Key::F6, "F6"},
    {Key::F7, "F7"},
    {Key::F8, "F8"},
    {Key::F9, "F9"},
    {Key::F10, "F10"},
    {Key::F11, "F11"},
    {Key::F12, "F12"},
}};

bool ascii_ci_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

std::string ascii_upper(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

std::string ascii_lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

struct ModifierToken {
    Modifier flag;
    std::string_view name;
};

// Parse/format order — fixed so format() round-trips through parse()
// byte-for-byte regardless of the order a caller happened to set bits
// in Modifier.
constexpr std::array<ModifierToken, 4> kModifierTokens{{
    {Modifier::Ctrl, "Ctrl"},
    {Modifier::Alt, "Alt"},
    {Modifier::Shift, "Shift"},
    {Modifier::Super, "Super"},
}};

}  // namespace

std::string_view key_name(Key key) noexcept {
    CKV_ASSERT(key != Key::Char && key != Key::None);
    for (const NamedKey& entry : kNamedKeys)
        if (entry.key == key) return entry.name;
    CKV_ASSERT(false);  // every non-Char, non-None Key must be in the table
    return {};
}

std::optional<Key> key_from_name(std::string_view name) noexcept {
    for (const NamedKey& entry : kNamedKeys)
        if (ascii_ci_equal(entry.name, name)) return entry.key;
    return std::nullopt;
}

std::string format(const KeyChord& chord) {
    CKV_ASSERT(chord.key != Key::None);
    std::string out;
    for (const ModifierToken& token : kModifierTokens) {
        if (has_modifier(chord.modifiers, token.flag)) {
            out += token.name;
            out += '+';
        }
    }
    if (chord.key == Key::Char) {
        out += chord.text.size() == 1 ? ascii_upper(chord.text) : chord.text;
    } else {
        out += key_name(chord.key);
    }
    return out;
}

std::optional<KeyChord> KeyChord::parse(std::string_view text) {
    if (text.empty()) return std::nullopt;

    Modifier modifiers = Modifier::None;
    std::string_view remaining = text;
    while (true) {
        const std::size_t plus = remaining.find('+');
        if (plus == std::string_view::npos) break;
        const std::string_view token = remaining.substr(0, plus);
        bool matched = false;
        for (const ModifierToken& candidate : kModifierTokens) {
            if (!ascii_ci_equal(candidate.name, token)) continue;
            modifiers = modifiers | candidate.flag;
            matched = true;
            break;
        }
        if (!matched) return std::nullopt;
        remaining = remaining.substr(plus + 1);
    }
    if (remaining.empty()) return std::nullopt;

    KeyChord result;
    result.modifiers = modifiers;
    if (const auto named = key_from_name(remaining)) {
        result.key = *named;
    } else {
        const std::vector<std::string_view> graphemes = text::split_graphemes(remaining);
        // Not a known name, and not a single character either.
        if (graphemes.size() != 1) return std::nullopt;
        result.key = Key::Char;
        result.text = ascii_lower(remaining);
    }
    return result;
}

}  // namespace ckv
