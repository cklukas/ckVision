// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A focused, command-safe shortcut editor. Starting capture is deliberate
// (Enter, Space, or pointer press); the next key press is then consumed by
// this control and published as a typed KeyChord instead of falling through
// to the application's command registry. Escape abandons capture, while
// Backspace/Delete clear an existing binding when the control is idle.
// Disabled (D-076), it shows its binding in "ckv.input.disabled" and cannot
// begin a capture.
#pragma once

#include <functional>
#include <optional>

#include "cvision/core/key.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// One row showing the bound chord in its display spelling, "Unbound" when
// there is none, or "Press a shortcut..." while capturing, clipped to the
// width. Resolves "ckv.input.normal", "ckv.input.focused" and
// "ckv.input.disabled" from context() once attached.
class KeyChordCapture : public ui::View {
public:
    // An unbound, idle control: a Tab stop one row high, preferring 18 cells.
    KeyChordCapture();

    // The binding shown, or nullopt for none. Setting it is the owner's own
    // change: it does not fire on_chord_changed and does not end a capture
    // in progress.
    void set_chord(std::optional<KeyChord> chord);
    const std::optional<KeyChord>& chord() const noexcept { return chord_; }

    // Whether the next key press will be taken as the binding. begin_capture
    // does nothing while disabled or already capturing; cancel_capture ends a
    // capture and keeps the binding; losing the focus cancels too. clear()
    // cancels any capture, removes the binding, and fires on_chord_changed
    // if there was one to remove.
    bool capturing() const noexcept { return capturing_; }
    void begin_capture();
    void cancel_capture();
    void clear();

    // Called after the reader changes the value, by capturing a chord (even
    // one equal to the old binding) or by clearing it, and after an explicit
    // clear(). Not called by set_chord(). The value is typed rather than a
    // display string so persistence and command-map policy remain
    // application-owned and independent of terminal spelling.
    std::function<void(const std::optional<KeyChord>&)> on_chord_changed;

    void on_attached() override;
    void on_focus(const FocusEvent& event) override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    // Handles presses only; repeats and releases are left unhandled. While
    // capturing, every press is consumed: Escape cancels, any other chord
    // that names a key, modifiers included, becomes the binding. While idle,
    // Enter or Space
    // (whatever the modifiers) begins a capture and Backspace or Delete
    // clears; other keys are left unhandled.
    bool on_key(const KeyEvent& event) override;
    // A primary-button press inside the control begins a capture; a press of
    // another button is left unhandled.
    bool on_mouse(const MouseEvent& event) override;
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    }

private:
    void publish_change();

    std::optional<KeyChord> chord_;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    bool capturing_ = false;
};

}  // namespace ckv::widgets
