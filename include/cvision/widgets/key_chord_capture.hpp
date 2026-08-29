// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A focused, command-safe shortcut editor. Starting capture is deliberate
// (Enter, Space, or pointer press); the next key press is then consumed by
// this control and published as a typed KeyChord instead of falling through
// to the application's command registry. Escape abandons capture, while
// Backspace/Delete clear an existing binding when the control is idle.
#pragma once

#include <functional>
#include <optional>

#include "cvision/core/key.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

class KeyChordCapture : public ui::View {
public:
    KeyChordCapture();

    void set_chord(std::optional<KeyChord> chord);
    const std::optional<KeyChord>& chord() const noexcept { return chord_; }

    bool capturing() const noexcept { return capturing_; }
    void begin_capture();
    void cancel_capture();
    void clear();

    // Called after the value changes, including an explicit clear. The value
    // is typed rather than a display string so persistence and command-map
    // policy remain application-owned and independent of terminal spelling.
    std::function<void(const std::optional<KeyChord>&)> on_chord_changed;

    void on_attached() override;
    void on_focus(const FocusEvent& event) override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    }

private:
    void publish_change();

    std::optional<KeyChord> chord_;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    bool capturing_ = false;
    bool focused_ = false;
};

}  // namespace ckv::widgets
