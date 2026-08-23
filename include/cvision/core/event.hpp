// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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
    WheelLeft,
    WheelRight,
};

enum class MouseAction : std::uint8_t {
    Down,
    Up,
    Move,
    Wheel,
    // A host-recognized double press. ckVision never infers this from elapsed
    // time, preserving deterministic core/UI behavior; backends that can
    // recognize it may deliver the semantic gesture explicitly.
    DoubleClick,
};

// A key press is the portable baseline. Kitty can additionally report
// hardware repeat and release transitions; those are routed separately so
// existing text controls and command bindings never mistake a release for a
// second activation.
enum class KeyAction : std::uint8_t {
    Press,
    Repeat,
    Release,
};

// One mouse model, two native coordinate spaces (the decision log D-018):
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
    KeyAction action = KeyAction::Press;
    // Whether a matching Release will follow this press. It is per-event,
    // not per-terminal: one kitty session can mix encodings, reporting
    // releases for the keys it escape-codes while Enter and Space still
    // arrive as legacy bytes that never report one — only a session whose
    // verified enhancements cover every key promises a release for all of
    // them. The decoder sets this from the enhancement set the host
    // verifiably honoured (D-055), never from what was requested. A control
    // that holds itself down until release must consult this, or it waits
    // forever for a release the key never sends.
    bool reports_release = false;

    friend bool operator==(const KeyEvent&, const KeyEvent&) = default;
};

// Composed IME/dead-key text, or the sanitized content of a bracketed
// paste (paste sets `from_paste`; the architecture §12 sanitization has
// already run by the time this event exists). `paste_recovered` reports
// conservative recovery from ambiguous/incomplete paste framing: its text is
// still safe paste text, never a sequence of synthetic key events.
struct TextEvent {
    std::string text;
    bool from_paste = false;
    bool paste_recovered = false;

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
