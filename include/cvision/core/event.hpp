// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "cvision/core/geometry.hpp"
#include "cvision/core/key.hpp"

namespace ckv {

// Which button a mouse report concerns. The wheel directions are buttons too,
// because that is how terminals report them; they accompany only
// MouseAction::Wheel, and the three physical buttons never do.
enum class MouseButton : std::uint8_t {
    // No button: pointer motion with nothing held, or a report whose button
    // the terminal did not identify.
    None = 0,
    // The three physical buttons, reported with Down, Up, DoubleClick, and
    // with Move while held during a drag.
    Left,
    Middle,
    Right,
    // One wheel notch in the named direction; Left and Right are horizontal
    // scrolling.
    WheelUp,
    WheelDown,
    WheelLeft,
    WheelRight,
};

// What happened to the pointer.
enum class MouseAction : std::uint8_t {
    // A button went down. Unless a popup already holds input capture,
    // Application routes every further mouse report to the view that
    // received the press until the next Up.
    Down,
    // A button was released; `button` names the released one when the
    // terminal says which, and None otherwise.
    Up,
    // The pointer moved. `button` is the one held during a drag, or None for
    // motion with no button held (reported only when the host tracks all
    // motion).
    Move,
    // One wheel notch; `button` carries the direction. Application offers an
    // unconsumed wheel event to the target's ancestors in turn.
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

// One mouse model, two native coordinate spaces (DECISIONS.md D-018):
// `cell` is always present; `pixel` is present exactly when the
// terminal reported pixel coordinates (SGR-Pixels). Never synthesized
// from `cell` — absence means the precision genuinely is not available.
struct MouseEvent {
    // The transition and the button it concerns; MouseAction says what
    // `button` means with each action.
    MouseAction action = MouseAction::Move;
    MouseButton button = MouseButton::None;
    // The 0-based cell (column x, row y) under the pointer, in frame-absolute
    // coordinates. Application delivers the event unchanged, so a view that
    // wants its own coordinates converts from its position on screen.
    Point cell;
    // The 0-based pixel position within the terminal's text area, present
    // only when the terminal reported pixels.
    std::optional<PixelPoint> pixel;
    // The modifiers held at the time of the report. The terminal decoder sets
    // only Shift, Alt and Ctrl: mouse reports carry no Super bit.
    Modifier modifiers = Modifier::None;

    // Memberwise equality, including whether `pixel` is present.
    friend bool operator==(const MouseEvent&, const MouseEvent&) = default;
};

// One keyboard event: the chord and the transition it reports. Press and
// Repeat take the ordinary key route (View::on_key, then command bindings);
// Release goes only to View::on_key_release.
struct KeyEvent {
    // The key, its modifiers and, for Key::Char, its text.
    KeyChord chord;
    // Press unless the host reported a repeat or a release, which only the
    // kitty keyboard protocol's event-type field does.
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

    // Memberwise equality, including the transition and `reports_release`.
    friend bool operator==(const KeyEvent&, const KeyEvent&) = default;
};

// Composed IME/dead-key text, or the sanitized content of a bracketed
// paste (paste sets `from_paste`; ARCHITECTURE.md §12 sanitization has
// already run by the time this event exists). `paste_recovered` reports
// conservative recovery from ambiguous/incomplete paste framing: its text is
// still safe paste text, never a sequence of synthetic key events.
struct TextEvent {
    // The UTF-8 text to insert; whether it came from a bracketed paste; and
    // the recovery flag described above, which the terminal decoder sets only
    // together with `from_paste`.
    std::string text;
    bool from_paste = false;
    bool paste_recovered = false;

    // Memberwise equality.
    friend bool operator==(const TextEvent&, const TextEvent&) = default;
};

// A change of keyboard focus. Application sends it to a view when that view
// loses (`gained` false) or receives (`gained` true) focus, and also forwards
// the terminal's own focus reports for the host window to the focused view,
// or to the active modal root when focus lies outside it.
struct FocusEvent {
    // True when focus arrived, false when it left.
    bool gained = false;

    // Memberwise equality.
    friend bool operator==(const FocusEvent&, const FocusEvent&) = default;
};

// The terminal changed size. Application answers by setting its root view's
// bounds to the new size.
struct ResizeEvent {
    // The new terminal size in cells: columns (width) by rows (height).
    Size cells;

    // Memberwise equality.
    friend bool operator==(const ResizeEvent&, const ResizeEvent&) = default;
};

// A timer expiry notice carrying the timer's identity. No backend and no part
// of Application produces or consumes it: it is not part of term::TerminalEvent,
// and Application::start_timer delivers expiries by invoking the callback
// given to it instead.
struct TimerEvent {
    // The identity of the timer that expired.
    std::uint64_t timer_id = 0;

    // Memberwise equality.
    friend bool operator==(const TimerEvent&, const TimerEvent&) = default;
};

}  // namespace ckv
