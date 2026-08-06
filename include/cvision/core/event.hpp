// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "cvision/core/geometry.hpp"
#include "cvision/core/key.hpp"

namespace ckv {

enum class MouseButton : std::uint8_t {
    None = 0,
    Left,
    Middle,
    Right,
    WheelUp,
    WheelDown,
};

enum class MouseAction : std::uint8_t {
    Down,
    Up,
    Move,
    Wheel,
};

// One mouse model, two native coordinate spaces (DECISIONS.md D-018):
// `cell` is always present; `pixel` is present exactly when the
// terminal reported pixel coordinates (SGR-Pixels). Never synthesized
// from `cell` — absence means the precision genuinely is not available.
struct MouseEvent {
    MouseAction action = MouseAction::Move;
    MouseButton button = MouseButton::None;
    Point cell;
    std::optional<PixelPoint> pixel;
    Modifier modifiers = Modifier::None;

    friend bool operator==(const MouseEvent&, const MouseEvent&) = default;
};

struct KeyEvent {
    KeyChord chord;

    friend bool operator==(const KeyEvent&, const KeyEvent&) = default;
};

// Composed IME/dead-key text, or the sanitized content of a bracketed
// paste (paste sets `from_paste`; ARCHITECTURE.md §12 sanitization has
// already run by the time this event exists).
struct TextEvent {
    std::string text;
    bool from_paste = false;

    friend bool operator==(const TextEvent&, const TextEvent&) = default;
};

struct FocusEvent {
    bool gained = false;

    friend bool operator==(const FocusEvent&, const FocusEvent&) = default;
};

struct ResizeEvent {
    Size cells;

    friend bool operator==(const ResizeEvent&, const ResizeEvent&) = default;
};

struct TimerEvent {
    std::uint64_t timer_id = 0;

    friend bool operator==(const TimerEvent&, const TimerEvent&) = default;
};

}  // namespace ckv
