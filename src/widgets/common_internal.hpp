// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/event.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets::detail {

inline bool is_press(const KeyEvent& event) noexcept { return event.action == KeyAction::Press; }

inline std::string_view trim_spaces(std::string_view text) noexcept {
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    return text;
}

// The text a key event types, or nothing: a printable character pressed
// without Alt, Ctrl or Super. Terminals report ordinary typing as Key::Char
// and only an IME or a paste as a TextEvent, so a control that takes typing
// reads both; Alt, Ctrl and Super characters stay chords so that commands
// still see them, and Shift is part of producing the text.
inline std::optional<std::string_view> typed_text(const KeyEvent& event) noexcept {
    if (event.chord.key != Key::Char || event.chord.text.empty()) return std::nullopt;
    if (has_modifier(event.chord.modifiers, Modifier::Alt) || has_modifier(event.chord.modifiers, Modifier::Ctrl) ||
        has_modifier(event.chord.modifiers, Modifier::Super))
        return std::nullopt;
    return std::string_view(event.chord.text);
}

// Where the caret after an entry of `entry` sits in a field `width` columns
// wide starting at `x`: just past its last column, and never past the field.
inline std::optional<CursorState> entry_caret(const ui::View& view, int x, int text_end, int width) {
    if (width <= 0) return std::nullopt;
    const Rect absolute = view.absolute_bounds();
    return CursorState{true, Point{absolute.x + x + std::clamp(text_end, 0, width - 1), absolute.y},
                       CursorShape::Bar, false};
}

// `value` moved `delta` steps of `step`, held to [minimum, maximum]. Worked in
// 64 bits, where a step or its sum with the value cannot overflow for any
// pair of ints, and clamped before it is narrowed back.
inline int stepped(int value, int delta, int step, int minimum, int maximum) noexcept {
    const std::int64_t moved = std::int64_t{value} + std::int64_t{delta} * step;
    return static_cast<int>(std::clamp<std::int64_t>(moved, minimum, maximum));
}

// The Desktop a view's popups open on: the nearest one above it.
inline Desktop* enclosing_desktop(ui::View& view) {
    for (ui::View* ancestor = view.parent(); ancestor != nullptr; ancestor = ancestor->parent())
        if (auto* desktop = dynamic_cast<Desktop*>(ancestor)) return desktop;
    return nullptr;
}

// The end of `text` that fits in `columns`, whole clusters only. A search
// field types at its end, so when the query outgrows the field the end is the
// part the reader is working on.
inline std::string tail_to_width(std::string_view text, int columns) {
    const std::vector<std::string_view> graphemes = text::split_graphemes(text);
    int used = 0;
    std::size_t first = graphemes.size();
    while (first > 0) {
        const int width = text::grapheme_width(graphemes[first - 1]);
        if (used + width > columns) break;
        used += width;
        --first;
    }
    std::string tail;
    for (std::size_t i = first; i < graphemes.size(); ++i) tail.append(graphemes[i]);
    return tail;
}

// `text` without its last grapheme cluster. Backspace takes back what one
// keystroke or one pasted character made, never a byte of it: a byte would
// leave half a UTF-8 scalar behind.
inline std::string without_last_grapheme(std::string_view text) {
    const std::vector<std::string_view> graphemes = text::split_graphemes(text);
    if (graphemes.empty()) return {};
    return std::string(text.substr(0, text.size() - graphemes.back().size()));
}

}  // namespace ckv::widgets::detail
