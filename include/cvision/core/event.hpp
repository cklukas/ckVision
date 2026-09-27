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
    // The three physical buttons, reported with Down and Up, and with Move
    // while held during a drag.
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
    // On a Down, the press's place in a run: 1 for a single press, 2 for the
    // second press of a double click -- the same button on the same cell
    // within ui::kDoubleClickIntervalNanos of the first, on the Application's
    // injected Clock. The press after a double click begins a new run; there
    // is no triple click. No terminal reports double clicks, so Application
    // counts them from ordinary presses and sets this on every event it
    // dispatches (1 on anything but a Down), replacing whatever the event
    // carried. A view reads it rather than timing presses itself.
    int click_count = 1;

    // The image-local mapping (D-018): the pixel of a picture `image_pixels` in size, shown
    // scaled over the frame-absolute cells `area` of a terminal whose cell is `cell_pixels`,
    // that is on screen at the reported pixel position. It is the source pixel the presenter
    // samples there, so a click and the picture agree. Empty without a reported pixel
    // position (it is never estimated from `cell`), when any extent or the metric is not
    // positive, and when the reported pixel lies outside `area`.
    std::optional<PixelPoint> image_pixel(Rect area, PixelSize cell_pixels, PixelSize image_pixels) const noexcept {
        if (!pixel || area.width <= 0 || area.height <= 0 || cell_pixels.width <= 0 || cell_pixels.height <= 0 ||
            image_pixels.width <= 0 || image_pixels.height <= 0)
            return std::nullopt;
        const std::int64_t shown_width = std::int64_t{area.width} * cell_pixels.width;
        const std::int64_t shown_height = std::int64_t{area.height} * cell_pixels.height;
        const std::int64_t x = std::int64_t{pixel->x} - std::int64_t{area.x} * cell_pixels.width;
        const std::int64_t y = std::int64_t{pixel->y} - std::int64_t{area.y} * cell_pixels.height;
        if (x < 0 || y < 0 || x >= shown_width || y >= shown_height) return std::nullopt;
        return PixelPoint{static_cast<int>(x * image_pixels.width / shown_width),
                          static_cast<int>(y * image_pixels.height / shown_height)};
    }

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

// Text to insert. From the terminal it is always the sanitized content of a
// bracketed paste (`from_paste` set; the architecture §12 sanitization has
// already run by the time this event exists). Typed characters, dead-key
// results and IME-composed text reach a view as unmodified Key::Char events,
// because a terminal delivers them as ordinary character bytes; an editable
// control turns those into a TextEvent of its own (`from_paste` clear), as it
// does for an internal clipboard paste. `paste_recovered` reports
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

}  // namespace ckv
